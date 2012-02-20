#define NODE_DEFINE_STRING_CONSTANT(target, constant)                            \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::String::New(constant),                               \
                static_cast<v8::PropertyAttribute>(                       \
                    v8::ReadOnly|v8::DontDelete))


#define THROW_ERROR(TYPE, STR)                                          \
	return ThrowException(Exception::TYPE(String::New(STR)));

#define REQ_ARGS(N)                                                     \
  if (args.Length() < (N))                                              \
    return ThrowException(Exception::TypeError(                         \
                             String::New("Expected " #N "arguments"))); 

#define REQ_OBJ_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsObject())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be an object")));   \
  Local<Object> VAR = Local<Object>::Cast(args[I]);                  


#define REQ_MSG_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsObject())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be an object")));   \
	DBusMessageWrap *VAR = ObjectWrap::Unwrap<DBusMessageWrap>(Local<Object>::Cast(args[I]));


#define OPT_STR_ARG(I, VAR, DEFAULT)                                    \
  const char* VAR;                                                              \
  if (args.Length() <= (I)) {                                           \
    VAR = (DEFAULT);                                                    \
  } else if (args[I]->IsString()) {                                      \
    VAR = *String::Utf8Value(args[I]->ToString());                                        \
  } else {                                                              \
    return ThrowException(Exception::TypeError(                         \
              String::New("Argument " #I " must be a string"))); \
  }

#define REQ_STR_ARG(I, VAR)                                             \
  const char* VAR;														\
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a string")));    \
  String::Utf8Value _##VAR(args[I]->ToString());		\
  VAR = *_##VAR;

#define REQ_INT_ARG(I, VAR)                                             \
  int VAR;                                                              \
  if (args.Length() <= (I) || !args[I]->IsInt32())                      \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be an integer")));  \
  VAR = args[I]->Int32Value();

#define REQ_BOOL_ARG(I, VAR)                                             \
  int VAR;                                                              \
  if (args.Length() <= (I) || !args[I]->IsBoolean())                      \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a boolean")));  \
  VAR = args[I]->BooleanValue();

#define OPT_BOOL_ARG(I, VAR, DEFAULT)\
  bool VAR;\
  if (args.Length() <= (I)) \
    VAR = (DEFAULT); \
  else if (args[I]->IsBoolean()) \
  	VAR = args[I]->BooleanValue(); \
  else \
      return ThrowException(Exception::TypeError( \
                  String::New("Argument " #I " must be a boolean")));  


#define REQ_FN_ARG(I, VAR)                                              \
  v8::Local<v8::Function> VAR;                                                           \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a function")));  \
  VAR = v8::Local<v8::Function>::Cast(args[I]);

#define REQ_BUF_ARG(I, VARBLOB, VARLEN)                                             \
  const char* VARBLOB;													\
  size_t VARLEN;	                                                    \
  if (args.Length() <= (I) || (!args[I]->IsString() && !Buffer::HasInstance(args[I])))                      \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be an buffer")));  \
 if (args[I]->IsString()) { \
	String::AsciiValue string(args[I]->ToString()); \
	length = string.length(); \
	blob = *string; \
} else if (Buffer::HasInstance(args[I])) { \
	Local<Object> bufferIn=args[I]->ToObject(); \
	length = Buffer::Length(bufferIn); \
	blob = Buffer::Data(bufferIn); \
}


                  
#define OPT_INT_ARG(I, VAR, DEFAULT)                                    \
  int VAR;                                                              \
  if (args.Length() <= (I)) {                                           \
    VAR = (DEFAULT);                                                    \
  } else if (args[I]->IsInt32()) {                                      \
    VAR = args[I]->Int32Value();                                        \
  } else {                                                              \
    return ThrowException(Exception::TypeError(                         \
              String::New("Argument " #I " must be an integer"))); \
  }


#define NODE_SET_GETTER(t, n, f) t->PrototypeTemplate()->SetAccessor(String::NewSymbol(n), f, NULL, Handle<Value>(), PROHIBITS_OVERWRITING, ReadOnly);
#define NODE_SET_GETTER_SETTER(t, n, g, s) t->PrototypeTemplate()->SetAccessor(String::NewSymbol(n), g, s, Handle<Value>(), PROHIBITS_OVERWRITING);

#define THIS_CONNECTION(args) ObjectWrap::Unwrap<DBusConnectionWrap>(args.This())

#define THIS_MESSAGE(args) ObjectWrap::Unwrap<DBusMessageWrap>(args.This())

