
#include <v8.h>
#include <node.h>

#include <dbus/dbus.h>

#include "util.h"

#include <cstring>

using namespace node;
using namespace v8;


static int dbus_messages_iter_size(DBusMessageIter *iter) {
  int msg_count = 0;
  
  while( dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
    msg_count++;
    dbus_message_iter_next(iter);
  }
  return msg_count;
}

static int dbus_messages_size(DBusMessage *message) {
  DBusMessageIter iter;
  int msg_count = 0;
  
  dbus_message_iter_init(message, &iter);

  if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR) {
    return 0;
  }

  while ((dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
    msg_count++;
    //go the next
    dbus_message_iter_next (&iter);
  }

  return msg_count; 
}


class DBusMessageWrap : ObjectWrap {
public:
	static Persistent<FunctionTemplate> constructorTemplate;
	
	DBusMessage* message;
	const char * signature;

	DBusMessageWrap() : ObjectWrap(), message(NULL), signature(NULL) {

	};

	~DBusMessageWrap() {
		dbus_message_unref(message);
	};

	operator DBusMessage* () const {
		return message;
	};

	static void Init(Handle<Object> target) {
		
		Local<FunctionTemplate> t = FunctionTemplate::New(New);
		constructorTemplate = Persistent<FunctionTemplate>::New(t);
		constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
		constructorTemplate->SetClassName(String::NewSymbol("DBusMessage"));
		NODE_SET_METHOD(target, "methodCall", methodCall);
		NODE_SET_METHOD(target, "methodReturn", methodReturn);
		NODE_SET_METHOD(target, "signal", signal);
		NODE_SET_METHOD(target, "error", error);

		NODE_SET_GETTER(t, "serial", serial);
		NODE_SET_GETTER(t, "type", type);
		NODE_SET_GETTER(t, "path", path);
		NODE_SET_GETTER_SETTER(t, "arguments", getArguments, setArguments);
		NODE_SET_GETTER_SETTER(t, "signature", getSignature, setSignature);
		NODE_SET_GETTER_SETTER(t, "error", getErrorName, setErrorName);

		NODE_SET_GETTER_SETTER(t, "interface", getInterface, setInterface);
		NODE_SET_GETTER_SETTER(t, "member", getMember, setMember);
	};

	static Handle<Value> New(const Arguments &args) {
		HandleScope scope;
		DBusMessageWrap* object = new DBusMessageWrap();
		object->Wrap(args.This());
		return args.This();
	};

	static Handle<Value> finalizeMessage(DBusMessage* message) {
		HandleScope scope;
		Local<Object> object = constructorTemplate->GetFunction()->NewInstance();
		DBusMessageWrap *wrap = ObjectWrap::Unwrap<DBusMessageWrap>(object);
		wrap->message = message;
		return scope.Close(object);
	};

	static Handle<Value> methodCall(const Arguments& args) {
		REQ_STR_ARG(0, destination);
		REQ_STR_ARG(1, path);
		REQ_STR_ARG(2, interface);
		REQ_STR_ARG(3, method);
		return finalizeMessage(dbus_message_new_method_call(destination, path, interface, method));
	};

	static Handle<Value> methodReturn(const Arguments& args) {
		REQ_MSG_ARG(0, origin);
		return finalizeMessage(dbus_message_new_method_return(*origin));
	};

	static Handle<Value> signal(const Arguments& args) {
		REQ_STR_ARG(0, path);
		REQ_STR_ARG(1, interface);
		REQ_STR_ARG(2, name);
		return finalizeMessage(dbus_message_new_signal(path, interface, name));
	};

	static Handle<Value> error(const Arguments& args) {
		REQ_MSG_ARG(0, origin);
		REQ_STR_ARG(1, name);
		REQ_STR_ARG(2, message);
		return finalizeMessage(dbus_message_new_error(*origin, name, message));
	};
 

	static Handle<Value> serial(Local<String> property, const AccessorInfo& info) {
		return Integer::New(dbus_message_get_serial(*THIS_MESSAGE(info)));
	};

	static Handle<Value> type(Local<String> property, const AccessorInfo& info) {
		return Integer::New(dbus_message_get_type(*THIS_MESSAGE(info)));
	};

	static Handle<Value> path(Local<String> property, const AccessorInfo& info) {
		return String::New(dbus_message_get_path(*THIS_MESSAGE(info)));
	};

	static Handle<Value> decodeBoolean(DBusMessageIter *iter) {
		dbus_bool_t value = false;
		dbus_message_iter_get_basic(iter, &value);
		return Boolean::New(value);	
	};

	static Handle<Value> decodeInteger(DBusMessageIter *iter) {
		dbus_uint64_t value = 0; 
		dbus_message_iter_get_basic(iter, &value);
		return Integer::New(value);
	};

	static Handle<Value> decodeDouble(DBusMessageIter *iter) {
		double value = 0;
		dbus_message_iter_get_basic(iter, &value);
		return Number::New(value);
	}

	static Handle<Value> decodeString(DBusMessageIter *iter) {
		const char *value;
		dbus_message_iter_get_basic(iter, &value); 
		return String::New(value);
	}

	static Handle<Value> decode(DBusMessageIter *iter) {
		switch (dbus_message_iter_get_arg_type(iter)) {
		
		case DBUS_TYPE_BOOLEAN: 
				return decodeBoolean(iter);
		
		case DBUS_TYPE_BYTE:
		case DBUS_TYPE_INT16:
		case DBUS_TYPE_UINT16:
		case DBUS_TYPE_INT32:
		case DBUS_TYPE_UINT32:
		case DBUS_TYPE_INT64:
		case DBUS_TYPE_UINT64: 
			return decodeInteger(iter);
		
		case DBUS_TYPE_DOUBLE:
			return decodeDouble(iter);

		case DBUS_TYPE_OBJECT_PATH:
		case DBUS_TYPE_SIGNATURE:
		case DBUS_TYPE_STRING: 
			return decodeString(iter);
		
		case DBUS_TYPE_ARRAY:
		case DBUS_TYPE_STRUCT:
		{
			DBusMessageIter internal_iter, internal_temp_iter;
			int count = 0;         

			//count the size of the array
			dbus_message_iter_recurse(iter, &internal_temp_iter);
			count = dbus_messages_iter_size(&internal_temp_iter);

			//create the result object
			Local<Array> resultArray = Array::New(count);
			count = 0;
			dbus_message_iter_recurse(iter, &internal_iter);

			do {
				//this is dict entry
				if (dbus_message_iter_get_arg_type(&internal_iter)  == DBUS_TYPE_DICT_ENTRY) {
					//Item is dict entry, it is exactly key-value pair
					DBusMessageIter dict_entry_iter;
					//The key 
					dbus_message_iter_recurse(&internal_iter, &dict_entry_iter);
					Handle<Value> key  = decode(&dict_entry_iter);
					//The value
					dbus_message_iter_next(&dict_entry_iter);
					Handle<Value> value = decode(&dict_entry_iter);
					//set the property
					resultArray->Set(key, value); 
				} 
				else {
					//Item is array
					Handle<Value> itemValue = decode(&internal_iter);
					resultArray->Set(count, itemValue);
					count++;
				}
			} while(dbus_message_iter_next(&internal_iter));
			//return the array object
			return resultArray;
		}
		
		case DBUS_TYPE_VARIANT: { 
			DBusMessageIter internal_iter;
			dbus_message_iter_recurse(iter, &internal_iter);
			Handle<Value> result = decode(&internal_iter);
			return result;
		}
			
		case DBUS_TYPE_DICT_ENTRY: 		
		case DBUS_TYPE_INVALID: 
		default: 
			//should return 'undefined' object
			return Undefined();
		  
		}
	}

	static char* signatureFromValue(Local<Value>& value) {
		if (value->IsTrue() || value->IsFalse() || value->IsBoolean() ) {
			return const_cast<char*>(DBUS_TYPE_BOOLEAN_AS_STRING);
		} else if (value->IsInt32()) {
			return const_cast<char*>(DBUS_TYPE_INT32_AS_STRING);
		} else if (value->IsUint32()) {
			return const_cast<char*>(DBUS_TYPE_UINT32_AS_STRING);
		} else if (value->IsNumber()) {
			return const_cast<char*>(DBUS_TYPE_DOUBLE_AS_STRING);
		} else if (value->IsString()) {
			return const_cast<char*>(DBUS_TYPE_STRING_AS_STRING);
		} else if (value->IsArray()) {
			return const_cast<char*>(DBUS_TYPE_ARRAY_AS_STRING);
		} else if (value->IsObject()) {
			return const_cast<char*>(DBUS_TYPE_STRUCT_AS_STRING);
		} else {
			return NULL;
		}
	}

	static bool encodeBoolean(int type, Local<Value> value, DBusMessageIter *iter) {
		dbus_bool_t data = value->BooleanValue();  
		if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &data)) {
			return false;
		}
		return true;
	}

	static bool encodeInteger(int type, Local<Value> value, DBusMessageIter *iter) {
		dbus_uint64_t data = value->IntegerValue();
		if (!dbus_message_iter_append_basic(iter, type, &data)) {
			return false;
		}
		return true;
	}

	static bool encodeString(int type, Local<Value> value, DBusMessageIter *iter) {
		String::Utf8Value data_val(value->ToString());
		const char *data = *data_val;
		if (!dbus_message_iter_append_basic(iter, type, &data)) {
			return false;
		}
		return true;
	}

	static bool encodeDouble(int type, Local<Value> value, DBusMessageIter *iter) {
		double data = value->NumberValue();
		if (!dbus_message_iter_append_basic(iter, type, &data)) {
			return false;
		}
		return true;
	}

	static bool encodeArray(int type, Local<Value> value, DBusMessageIter *iter, DBusSignatureIter* siter) {

		if (dbus_signature_iter_get_element_type(siter) == DBUS_TYPE_DICT_ENTRY) {
			//This element is a DICT type of D-Bus
			if (! value->IsObject()) {
				return false;
			}
		
			Local<Object> value_object = value->ToObject();
			DBusMessageIter subIter;
			DBusSignatureIter dictSiter, dictSubSiter;
			char *dict_sig;

			dbus_signature_iter_recurse(siter, &dictSiter);
			dict_sig = dbus_signature_iter_get_signature(&dictSiter);

			if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, dict_sig, &subIter)) {
				dbus_free(dict_sig); 
				return false; 
			}
			dbus_free(dict_sig);

			Local<Array> prop_names = value_object->GetPropertyNames();
			int len = prop_names->Length();

			//for each signature
			dbus_signature_iter_recurse(&dictSiter, &dictSubSiter); //the key
			dbus_signature_iter_next(&dictSubSiter); //the value

			bool no_error_status = true;
			for(int i=0; i<len; i++) {
				DBusMessageIter dict_iter;

				if (!dbus_message_iter_open_container(&subIter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter)) {
					return false;
				}

				Local<Value> prop_name = prop_names->Get(i);
				//TODO: we currently only support 'string' type as dict key type
				//      for other type as arugment, we are currently not support
				String::Utf8Value prop_name_val(prop_name->ToString());
				char *prop_str = *prop_name_val;
				//append the key
				dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &prop_str);

				//append the value 
				char *cstr = dbus_signature_iter_get_signature(&dictSubSiter);
				if ( ! encode(value_object->Get(prop_name), &dict_iter, cstr)) {
					no_error_status = false;
				}

			//release resource
			dbus_free(cstr);
			dbus_message_iter_close_container(&subIter, &dict_iter); 
			//error on encode message, break and return
			if (!no_error_status) 
				return false;
		}
		dbus_message_iter_close_container(iter, &subIter);
		//Check errors on message
		if (!no_error_status) 
			return no_error_status;
		} else {
			//This element is a Array type of D-Bus 
			if (! value->IsArray()) {
				return false;
			}
		
			DBusMessageIter subIter;
			DBusSignatureIter arraySIter;
			char *array_sig = NULL;

			dbus_signature_iter_recurse(siter, &arraySIter);
			array_sig = dbus_signature_iter_get_signature(&arraySIter);

			if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, array_sig, &subIter)) {
				delete (array_sig); 
				return false; 
			}

			Local<Array> arrayData = Local<Array>::Cast(value);
			bool no_error_status = true;
			for (unsigned int i=0; i < arrayData->Length(); i++) {
				Local<Value> arrayItem = arrayData->Get(i);
				if (!encode(arrayItem, &subIter, array_sig) ) {
					no_error_status = false;
					break;
				}
			}
			dbus_message_iter_close_container(iter, &subIter);
			delete (array_sig);
			return no_error_status;
		}
	}

	static bool encodeVariant(int type, Local<Value> value, DBusMessageIter *iter, DBusSignatureIter* siter) {
		DBusMessageIter sub_iter;
		DBusSignatureIter var_siter;
		//FIXME: the variable stub
		char *var_sig = signatureFromValue(value);

		dbus_signature_iter_recurse(siter, &var_siter);

		if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, var_sig, &sub_iter)) {
			return false;
		}

		//encode the object to dbus message 
		if (!encode(value, &sub_iter, var_sig)) { 
			dbus_message_iter_close_container(iter, &sub_iter);
			return false;
		}
		dbus_message_iter_close_container(iter, &sub_iter);
		return true;
	}

	static bool encodeStruct(int type, Local<Value> value, DBusMessageIter *iter, DBusSignatureIter* siter) {
		DBusMessageIter sub_iter;
		DBusSignatureIter struct_siter;

		if (!dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &sub_iter)) {
			return false;
		}

		Local<Object> value_object = value->ToObject();

		Local<Array> prop_names = value_object->GetPropertyNames();
		int len = prop_names->Length(); 
		bool no_error_status = true;

		dbus_signature_iter_recurse(siter, &struct_siter);
		for(int i=0 ; i<len; i++) {
			char *sig = dbus_signature_iter_get_signature(&struct_siter);
			Local<Value> prop_name = prop_names->Get(i);

			if (!encode(value_object->Get(prop_name), &sub_iter, sig) ) {
				no_error_status = false;
			}

			dbus_free(sig);

			if (!dbus_signature_iter_next(&struct_siter) || !no_error_status) {
				break;
			}
		}
		dbus_message_iter_close_container(iter, &sub_iter);
		return no_error_status;
	}

	static bool encode(Local<Value> value, DBusMessageIter *iter, const char* sig) {
		
		DBusSignatureIter siter;
		dbus_signature_iter_init(&siter, sig);

		int type;

		switch ((type=dbus_signature_iter_get_current_type(&siter))) 
		{
		case DBUS_TYPE_BOOLEAN: 
			return encodeBoolean(type, value, iter);
		
		case DBUS_TYPE_INT16:
		case DBUS_TYPE_INT32:
		case DBUS_TYPE_INT64:
		case DBUS_TYPE_UINT16:
		case DBUS_TYPE_UINT32:
		case DBUS_TYPE_UINT64:
		case DBUS_TYPE_BYTE: 
			return encodeInteger(type, value, iter);
		
		case DBUS_TYPE_STRING: 
		case DBUS_TYPE_OBJECT_PATH:
		case DBUS_TYPE_SIGNATURE:
			return encodeString(type, value, iter);

		case DBUS_TYPE_DOUBLE:
			return encodeDouble(type, value, iter);

		case DBUS_TYPE_ARRAY: 
			return encodeArray(type, value, iter, &siter);
			
		
		case DBUS_TYPE_VARIANT: 
			return encodeVariant(type, value, iter, &siter);

		case DBUS_TYPE_STRUCT: 
			return encodeStruct(type, value, iter, &siter);
		
		default: 
			printf("Unknown type!\n");
			return false;
		}
		return true; 
	}

	static Handle<Value> getArguments(Local<String> property, const AccessorInfo& info) {
		DBusMessage *message = *THIS_MESSAGE(info);
		DBusMessageIter iter;
		int type;
		int argument_count = 0;
		int count;

		if ((count=dbus_messages_size(message)) <=0 ) {
			return Undefined();
		}     

		Local<Array> resultArray = Array::New(count);
		dbus_message_iter_init(message, &iter);

		while ((type=dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
			Handle<Value> valueItem = decode(&iter);
			resultArray->Set(argument_count, valueItem);
			//for next message
			dbus_message_iter_next (&iter);
			argument_count++;
		} //end of while loop

		return resultArray; 
	}
	
	static void setArguments(Local<String> property, Local<Value> value, const AccessorInfo& info) {
		DBusError error;
		DBusMessageWrap *wrap = THIS_MESSAGE(info);
		DBusMessage *message = *wrap;
		DBusMessageIter iter;
		DBusSignatureIter siter;
		uint32_t count = 0;
		const char* signature = wrap->signature;

		 dbus_error_init(&error);        
		if (!dbus_signature_validate(signature, &error)) {
			printf("Invalid signature: %s\n",error.message);
		}
		
		dbus_signature_iter_init(&siter, signature);
		dbus_message_iter_init_append(message, &iter);

		if (!value->IsArray()) {
			printf("NO ARRAY!");
		}

		Local<Array> arguments = Local<Array>::Cast(value);

		do {
			char *arg_sig = dbus_signature_iter_get_signature(&siter);
			

			if (arg_sig[0] == '\0')
				break;

			//process the argument sig
			if (count >= arguments->Length()) {
				printf("Arguments Not Enough (need %i, got %i)\n", count, arguments->Length());
				break;
			}


			//encode to message with given v8 Objects and the signature
			if (! encode(arguments->Get(count), &iter, arg_sig)) {
				printf("ERRORZ\n");
				dbus_free(arg_sig);
				break;
			}

			dbus_free(arg_sig);  
			count++;
		} while (dbus_signature_iter_next(&siter));
		
		

	};

	static Handle<Value> getSignature(Local<String> property, const AccessorInfo& info) {
		return String::New(THIS_MESSAGE(info)->signature);
	}

	static void setSignature(Local<String> property,  Local<Value> value, const AccessorInfo& info) {
		if (value->IsString()) {
			String::Utf8Value derp(value);			
			THIS_MESSAGE(info)->signature = strdup(*derp);
		}
		else {
			printf("FUUU\n");
		}
		
	}

	static Handle<Value> getErrorName(Local<String> property, const AccessorInfo& info) {
		return String::New(dbus_message_get_error_name(*THIS_MESSAGE(info)));
	};

	static void setErrorName(Local<String> property,  Local<Value> value, const AccessorInfo& info) {
		dbus_message_set_error_name(*THIS_MESSAGE(info), *String::Utf8Value(value->ToString()));
	};

	static Handle<Value> getMember(Local<String> property, const AccessorInfo& info) {
		DBusMessage* message = *THIS_MESSAGE(info);
		int type = dbus_message_get_type(message) ;
		if (type == DBUS_MESSAGE_TYPE_METHOD_CALL || type == DBUS_MESSAGE_TYPE_METHOD_RETURN || type == DBUS_MESSAGE_TYPE_SIGNAL)
			return String::New(dbus_message_get_member(*THIS_MESSAGE(info)));
		else
			return Undefined();
	};

	static void setMember(Local<String> property,  Local<Value> value, const AccessorInfo& info) {
		dbus_message_set_member (*THIS_MESSAGE(info), *String::Utf8Value(value->ToString()));
	};

	static Handle<Value> getInterface(Local<String> property, const AccessorInfo& info) {
		DBusMessage* message = *THIS_MESSAGE(info);
		int type = dbus_message_get_type(message) ;
		if (type == DBUS_MESSAGE_TYPE_METHOD_CALL || type == DBUS_MESSAGE_TYPE_METHOD_RETURN || type == DBUS_MESSAGE_TYPE_SIGNAL)
			return String::New(dbus_message_get_interface(message));
		else
			return Undefined();
	};

	static void setInterface(Local<String> property,  Local<Value> value, const AccessorInfo& info) {
		dbus_message_set_interface(*THIS_MESSAGE(info), *String::Utf8Value(value->ToString()));
	};
	 


};
Persistent<FunctionTemplate> DBusMessageWrap::constructorTemplate;

class DBusConnectionWrap : ObjectWrap {
public:

	static Persistent<FunctionTemplate> constructorTemplate;

	DBusConnection* connection;
	bool priv;
	
	
	DBusConnectionWrap(DBusConnection* c, bool p) : ObjectWrap(), connection(c), priv(p) {
		
	};
	
	~DBusConnectionWrap() {
		
		printf("dtor");
	};
	
	operator DBusConnection* () const {
		return connection;
	};


	static void Init(Handle<Object> target) {
		
		Local<FunctionTemplate> t = FunctionTemplate::New(New);
		constructorTemplate = Persistent<FunctionTemplate>::New(t);
		constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
		constructorTemplate->SetClassName(String::NewSymbol("DBusConnection"));

		NODE_SET_METHOD(target, "open", open);
		NODE_SET_METHOD(target, "get", get);

		NODE_SET_PROTOTYPE_METHOD(t, "close", close);
		NODE_SET_PROTOTYPE_METHOD(t, "requestName", requestName);

		//NODE_SET_PROTOTYPE_METHOD(t, "close", close);
		NODE_SET_PROTOTYPE_METHOD(t, "canSendType", canSendType);
		
		NODE_SET_PROTOTYPE_METHOD(t, "addFilter", addFilter);
		//NODE_SET_PROTOTYPE_METHOD(t, "removeFilter", removeFilter);

		NODE_SET_PROTOTYPE_METHOD(t, "registerObjectPath", registerObjectPath);
		NODE_SET_PROTOTYPE_METHOD(t, "unregisterObjectPath", unregisterObjectPath);

		NODE_SET_PROTOTYPE_METHOD(t, "send", send);

		NODE_SET_GETTER(t, "isConnected", isConnected);
		NODE_SET_GETTER(t, "isAuthenticated", isAuthenticated);
		NODE_SET_GETTER(t, "isAnonymous", isAnonymous);
		NODE_SET_GETTER(t, "serverId", serverId);
		
	};

	class ConnectionCallbackBaton {
	public:
		ConnectionCallbackBaton(Persistent<Function> cb, DBusConnectionWrap* conn) : callback(cb), connection(conn) { };
		~ConnectionCallbackBaton() { /*callback.Dispose();*/ };
		Persistent<Function> callback;
		DBusConnectionWrap* connection;
	};

	class DispatchBaton {
	public:
		DispatchBaton(DBusConnectionWrap* conn, uv_async_cb cb) : connection(conn) { 
			uv_async_init(uv_default_loop(), &this->work, cb);
			this->work.data = this;
		};
		DBusConnectionWrap* connection;
		uv_async_t work;
	};

	template<class T> class ConnectionCallbackWorkBaton {
	public:
		ConnectionCallbackWorkBaton(ConnectionCallbackBaton* c, T d, uv_async_cb cb) : callbackBaton(c), data(d) { 
			uv_async_init(uv_default_loop(), &this->work, cb);
			this->work.data = this;
		};
		~ConnectionCallbackWorkBaton() { };
		ConnectionCallbackBaton* callbackBaton;
		T data;
		uv_async_t work;

	};

	static Handle<Value> New(const Arguments &args) {
		HandleScope scope;
		DBusConnectionWrap* object = new DBusConnectionWrap(NULL, false);
		object->Wrap(args.This());
		return args.This();
	};

	static Handle<Value> close(const Arguments &args) {
		DBusConnectionWrap* connection = THIS_CONNECTION(args);
		if (connection->priv &&  dbus_connection_get_is_connected(*connection))
			dbus_connection_close(*connection);
		dbus_connection_unref(*connection);
	};



	static void dispatch(uv_async_t* work, int status) {
		DispatchBaton* baton = static_cast<DispatchBaton*>(work->data);
		DBusConnection* connection = *baton->connection;
		while(dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS);
		//delete baton;
	}

	static void dispatchStatus(DBusConnection *connection, DBusDispatchStatus status, void *data) {
		DBusConnectionWrap* wrap = ((DBusConnectionWrap*)data);
		if (status == DBUS_DISPATCH_DATA_REMAINS) {
			DispatchBaton* baton = new DispatchBaton(wrap, dispatch);
			uv_async_send(&baton->work);
		}
	};

	static void freeWatchData(void* data) {
		ev_io* io = static_cast<ev_io*>(data);
		delete io;
	};

	static void watchCallback(struct ev_loop *loop, ev_io *io, int events) {
		DBusWatch *watch = static_cast<DBusWatch*>(io->data);
		int flags = dbus_watch_get_flags(watch);
		while (!dbus_watch_handle(watch, 
			(events & EV_READ ? DBUS_WATCH_READABLE : 0) | 
			(events & EV_WRITE ? DBUS_WATCH_WRITABLE : 0) |
			(events & EV_ERROR ? DBUS_WATCH_ERROR | DBUS_WATCH_HANGUP : 0)
			)
		);
	}

	static void configureWatch(DBusWatch *watch) {
		ev_io* io = static_cast<ev_io*>(dbus_watch_get_data(watch));
		if (dbus_watch_get_enabled(watch)) {
			int flags = dbus_watch_get_flags(watch);
			ev_io_set(io, dbus_watch_get_socket(watch), 
				(flags & DBUS_WATCH_READABLE ? EV_READ : 0) | 
				(flags & DBUS_WATCH_WRITABLE ? EV_WRITE : 0)
			);
		}
		else {
			ev_io_set(io, dbus_watch_get_unix_fd(watch), 0);
		}
	}
	
	static dbus_bool_t addWatch(DBusWatch *watch, void *data) {
		ev_io* io = new ev_io();
		ev_io_init(io, watchCallback, dbus_watch_get_unix_fd(watch), EV_READ | EV_WRITE);
		io->data = watch;
		dbus_watch_set_data(watch, io, freeWatchData);
		configureWatch(watch);
		ev_io_start(ev_default_loop(0), io);
		return true;
	};
	
	static void removeWatch(DBusWatch *watch, void *data) {
		ev_io_stop(ev_default_loop(0), static_cast<ev_io*>(dbus_watch_get_data(watch)));
	};
	
	static void watchToggled(DBusWatch *watch, void *data) {
		configureWatch(watch);
	};



	static void freeTimeoutData(void* data) {
		uv_timer_t* timer = static_cast<uv_timer_t*>(data);
		//delete timer;
	};

	static void timerCallback(uv_timer_t* timer, int status) {
		dbus_timeout_handle((DBusTimeout*)timer->data);
	};

	static void configureTimeout(DBusTimeout *timeout) {
		uv_timer_t* timer = static_cast<uv_timer_t*>(dbus_timeout_get_data(timeout));
		if (dbus_timeout_get_enabled(timeout)) {
			uv_timer_start(timer, timerCallback, dbus_timeout_get_interval(timeout), 0);
		}
		else {
			uv_timer_stop(timer);
		}
	}

	static dbus_bool_t addTimeout(DBusTimeout *timeout, void *data) {
		uv_timer_t* timer = new uv_timer_t();
		uv_timer_init(uv_default_loop(), timer);
		dbus_timeout_set_data(timeout, timer, freeTimeoutData);
		timer->data = timeout;
		configureTimeout(timeout);
		return true;
	};
	
	static void removeTimeout(DBusTimeout *timeout, void *data) {
		uv_timer_t* timer = static_cast<uv_timer_t*>(dbus_timeout_get_data(timeout));
		uv_timer_stop(timer);
	};
	
	static void timeoutToggled(DBusTimeout *timeout, void *data) {
		configureTimeout(timeout);
	};


	

	static Handle<Value> finalizeConnection(DBusConnection* connection, bool priv) {

		

		HandleScope scope;
		Local<Object> object = constructorTemplate->GetFunction()->NewInstance();
		DBusConnectionWrap *wrap = ObjectWrap::Unwrap<DBusConnectionWrap>(object);
		wrap->connection = connection;
		wrap->priv = priv;

		dbus_connection_set_dispatch_status_function(connection, dispatchStatus, wrap, NULL);
		dbus_connection_set_watch_functions(connection, addWatch, removeWatch, watchToggled, wrap, NULL);
		dbus_connection_set_timeout_functions(connection, addTimeout, removeTimeout, timeoutToggled, wrap, NULL);

		dispatchStatus(connection, dbus_connection_get_dispatch_status(connection), wrap);
		

		

		//printf("Functions have been set!\n");

		wrap->Ref();
	
		return scope.Close(object);
	}

	

	static void freeConnectionCallbackBaton(void* data) {
		//delete (ConnectionCallbackBaton*)data;
	}


	static void handleMessageUserland(uv_async_t* work, int status) {
		HandleScope scope;
		ConnectionCallbackWorkBaton<DBusMessage*>* baton = static_cast<ConnectionCallbackWorkBaton<DBusMessage*>*>(work->data);
		Handle<Value> argv[1] = { DBusMessageWrap::finalizeMessage(baton->data) };
		TryCatch tryCatch;
		baton->callbackBaton->callback->Call(Context::GetCurrent()->Global(), 1, argv);
		if (tryCatch.HasCaught())
			FatalException(tryCatch);
		
	}

	static DBusHandlerResult handleMessage(DBusConnection* connection, DBusMessage* message, void* data) {
		ConnectionCallbackWorkBaton<DBusMessage*>* baton = new ConnectionCallbackWorkBaton<DBusMessage*>(static_cast<ConnectionCallbackBaton*>(data), message, handleMessageUserland);
		dbus_message_ref(message);
		uv_async_send(&baton->work);
		return DBUS_HANDLER_RESULT_HANDLED;
	};

	static void unregister(DBusConnection *connection, void *user_data) {

	}

	static Handle<Value> addFilter(const Arguments &args) {
		REQ_FN_ARG(0, callback);
		DBusConnectionWrap* connection = THIS_CONNECTION(args);
		ConnectionCallbackBaton* baton = new ConnectionCallbackBaton(Persistent<Function>::New(callback), connection);
		if (!dbus_connection_add_filter(*connection, handleMessage, baton, freeConnectionCallbackBaton))
				THROW_ERROR(Error, "Unable to add connection filter!");
		return True();
	};



	static Handle<Value> registerObjectPath(const Arguments &args) {
		REQ_STR_ARG(0, path);
		REQ_FN_ARG(1, callback);

		DBusError error;
		DBusConnectionWrap* connection = THIS_CONNECTION(args);
		DBusObjectPathVTable *vtable = new DBusObjectPathVTable();
		ConnectionCallbackBaton* baton = new ConnectionCallbackBaton(Persistent<Function>::New(callback), connection);

		vtable->message_function = handleMessage;
		vtable->unregister_function = unregister;



		dbus_error_init(&error);
		if (!dbus_connection_try_register_object_path(*connection, path, vtable, baton, &error)) {
			THROW_ERROR(Error, "Unable to add connection filter!");
		}		
				
		return True();
	};

	static Handle<Value> unregisterObjectPath(const Arguments &args) {

	}


	static void pendingCallNotifyCallbackUserland(uv_async_t* work, int status) {

		ConnectionCallbackWorkBaton<DBusPendingCall*>* baton = static_cast<ConnectionCallbackWorkBaton<DBusPendingCall*>*>(work->data);
		DBusPendingCall *pending = baton->data;

		DBusMessage* reply = dbus_pending_call_steal_reply(pending);		
		HandleScope scope;
		TryCatch tryCatch;
		Local<Value> argv[] = { Local<Value>::New(DBusMessageWrap::finalizeMessage(reply)) };
		baton->callbackBaton->callback->Call(Context::GetCurrent()->Global(), 1, argv);
		if (tryCatch.HasCaught())
			FatalException(tryCatch);
		dbus_pending_call_unref(pending);
			
			
		

		//delete baton;
	}

	static void pendingCallNotifyCallback(DBusPendingCall *pending, void *data) {
		ConnectionCallbackWorkBaton<DBusPendingCall*>* baton = new ConnectionCallbackWorkBaton<DBusPendingCall*>(static_cast<ConnectionCallbackBaton*>(data), pending, pendingCallNotifyCallbackUserland);
		uv_async_send(&baton->work);
	};


	static Handle<Value> send(const Arguments &args) {
		DBusConnectionWrap* connection = THIS_CONNECTION(args);
		REQ_MSG_ARG(0, message);

		dbus_uint32_t serial;
		
		int timeout = -1;

		switch(args.Length()) {
		//message
		case 1:
			dbus_connection_send(*connection, *message, &serial);
			break;
		//message, callback
		//case 2:
		//	REQ_FN_ARG(1, callback);
		//message, timeout, callback
		case 3:
			REQ_INT_ARG(1, timeout);
			REQ_FN_ARG(2, callback);

			DBusPendingCall* pendingCall = NULL;
			

			if (!dbus_connection_send_with_reply(*connection, *message, &pendingCall, timeout)) {
				printf("FUUUU\n");
			}

			if (pendingCall == NULL) {
				printf("FUUUUUU\n");
				break;
			}

			if (!dbus_pending_call_get_completed(pendingCall)) {
				ConnectionCallbackBaton* baton = new ConnectionCallbackBaton(Persistent<Function>::New(callback), connection);
				dbus_pending_call_set_notify(pendingCall, pendingCallNotifyCallback, baton, freeConnectionCallbackBaton);
			}
			else {
				printf("DURR RESPONSE\n");
			}
			break;
		
		}
		return Undefined();
	};

	static Handle<Value> removeFilter(const Arguments &args) {
		return Undefined();
	}

	static Handle<Value> get(const Arguments &args) {
		REQ_INT_ARG(0, type);
		OPT_BOOL_ARG(1, priv, false)
		DBusError error;
		dbus_error_init(&error);
		DBusConnection* connection = !priv ? 
			dbus_bus_get(DBusBusType(type), &error) : 
			dbus_bus_get_private(DBusBusType(type), &error);
		

		if (!dbus_error_is_set(&error)) {
			dbus_connection_set_exit_on_disconnect(connection, false);
			return finalizeConnection(connection, priv);
		}

		THROW_ERROR(Error, error.message);
		
	}

	static Handle<Value> open(const Arguments &args) {
		REQ_STR_ARG(0, address);
		OPT_BOOL_ARG(1, priv, false)
		DBusError error;
		dbus_error_init(&error);
		DBusConnection* connection = !priv ? dbus_connection_open(address, &error) : dbus_connection_open_private(address, &error);
		
		if (!dbus_error_is_set(&error)) {
			return finalizeConnection(connection, priv);
		}

		THROW_ERROR(Error, error.message);

	};

	static Handle<Value> requestName(const Arguments &args) {
		REQ_STR_ARG(0, name);
		OPT_INT_ARG(1, opts, 0);
		int result;
		DBusError error;
		
		dbus_error_init(&error);
		result = dbus_bus_request_name(*THIS_CONNECTION(args), name, opts, &error);

		if (!dbus_error_is_set(&error))
			return Integer::New(result);

		THROW_ERROR(Error, error.message);
	};

	static Handle<Value> canSendType(const Arguments &args) {
		REQ_INT_ARG(0, type);
		return Boolean::New(dbus_connection_can_send_type(*THIS_CONNECTION(args), type));
	};

	/*
	static Handle<Value> send(const Arguments &args) {
		dbus_connection_send(connection);
	};

	static Handle<Value> sendWithReply(const Arguments &args) {
		dbus_connection_send(connection);
	};

	static Handle<Value> registerObjectPath(const Arguments &args) {
		dbus_connection_send(connection);
	};

	static Handle<Value> registerFallbackPath(const Arguments &args) {
		dbus_connection_send(connection);
	};*/

	static Handle<Value> serverId(Local<String> property, const AccessorInfo& info) {
		return String::New(dbus_connection_get_server_id(*THIS_CONNECTION(info)));
	};

	static Handle<Value> isConnected(Local<String> property, const AccessorInfo& info) {
		return Boolean::New(dbus_connection_get_is_connected(*THIS_CONNECTION(info)));
	};

	static Handle<Value> isAuthenticated(Local<String> property, const AccessorInfo& info) {
		return Boolean::New(dbus_connection_get_is_authenticated(*THIS_CONNECTION(info)));
	};

	static Handle<Value> isAnonymous(Local<String> property, const AccessorInfo& info) {
		return Boolean::New(dbus_connection_get_is_anonymous(*THIS_CONNECTION(info)));
	};
};
Persistent<FunctionTemplate> DBusConnectionWrap::constructorTemplate;



extern "C" {
	
	static void init (Handle<Object> target)
	{
				
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_PATH_DBUS);
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_PATH_LOCAL);
		
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_INTERFACE_DBUS);
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_INTERFACE_INTROSPECTABLE);
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_INTERFACE_PROPERTIES);
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_INTERFACE_PEER);
		NODE_DEFINE_STRING_CONSTANT(target, DBUS_INTERFACE_LOCAL);

		NODE_DEFINE_CONSTANT(target, DBUS_NAME_FLAG_ALLOW_REPLACEMENT);
		NODE_DEFINE_CONSTANT(target, DBUS_NAME_FLAG_REPLACE_EXISTING);
		NODE_DEFINE_CONSTANT(target, DBUS_NAME_FLAG_DO_NOT_QUEUE);
		
		NODE_DEFINE_CONSTANT(target, DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
		NODE_DEFINE_CONSTANT(target, DBUS_REQUEST_NAME_REPLY_IN_QUEUE);
		NODE_DEFINE_CONSTANT(target, DBUS_REQUEST_NAME_REPLY_EXISTS);
		NODE_DEFINE_CONSTANT(target, DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER);
		
		NODE_DEFINE_CONSTANT(target, DBUS_RELEASE_NAME_REPLY_RELEASED);
		NODE_DEFINE_CONSTANT(target, DBUS_RELEASE_NAME_REPLY_NON_EXISTENT);
		NODE_DEFINE_CONSTANT(target, DBUS_RELEASE_NAME_REPLY_NOT_OWNER);

		NODE_DEFINE_CONSTANT(target, DBUS_START_REPLY_SUCCESS);
		NODE_DEFINE_CONSTANT(target, DBUS_START_REPLY_ALREADY_RUNNING);

		NODE_DEFINE_CONSTANT(target, DBUS_BUS_SESSION);
		NODE_DEFINE_CONSTANT(target, DBUS_BUS_SYSTEM);
		NODE_DEFINE_CONSTANT(target, DBUS_BUS_STARTER);


		NODE_DEFINE_CONSTANT(target, DBUS_HANDLER_RESULT_HANDLED);
		NODE_DEFINE_CONSTANT(target, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
		NODE_DEFINE_CONSTANT(target, DBUS_HANDLER_RESULT_NEED_MEMORY);

		//NODE_DEFINE_STRING_CONSTANT(target, DBUS_LITTLE_ENDIAN);
		//NODE_DEFINE_STRING_CONSTANT(target, DBUS_BIG_ENDIAN);
		
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_INVALID);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_BYTE);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_BOOLEAN);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_INT16);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_UINT16);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_INT32);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_UINT32);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_INT64);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_UINT64);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_DOUBLE);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_STRING);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_OBJECT_PATH);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_SIGNATURE);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_UNIX_FD);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_ARRAY);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_VARIANT);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_STRUCT);
		NODE_DEFINE_CONSTANT(target, DBUS_TYPE_DICT_ENTRY);

		NODE_DEFINE_CONSTANT(target, DBUS_STRUCT_BEGIN_CHAR);
		NODE_DEFINE_CONSTANT(target, DBUS_STRUCT_END_CHAR);
		NODE_DEFINE_CONSTANT(target, DBUS_DICT_ENTRY_BEGIN_CHAR);
		NODE_DEFINE_CONSTANT(target, DBUS_DICT_ENTRY_END_CHAR);

		NODE_DEFINE_CONSTANT(target, DBUS_MAXIMUM_NAME_LENGTH);
		NODE_DEFINE_CONSTANT(target, DBUS_MAXIMUM_SIGNATURE_LENGTH );

		NODE_DEFINE_CONSTANT(target, DBUS_NUMBER_OF_TYPES);

		NODE_DEFINE_CONSTANT(target, DBUS_MESSAGE_TYPE_METHOD_CALL);
		NODE_DEFINE_CONSTANT(target, DBUS_MESSAGE_TYPE_METHOD_RETURN);
		NODE_DEFINE_CONSTANT(target, DBUS_MESSAGE_TYPE_SIGNAL);
		NODE_DEFINE_CONSTANT(target, DBUS_MESSAGE_TYPE_ERROR);
		NODE_DEFINE_CONSTANT(target, DBUS_MESSAGE_TYPE_INVALID);
		

		DBusConnectionWrap::Init(target);
		DBusMessageWrap::Init(target);
	}

	NODE_MODULE(dbus, init)
}
	