
var 
	DOM = require('dom'),
	util = require('util'),
	dbus = require('./build/Release/dbus'),
	EventEmitter = require('events').EventEmitter;





function DBus(bus, destination) {
	switch(typeof bus) {
	case "number":
		this.backend = dbus.get(bus);
		break;
	case "object":
		this.backend = bus;
		break;
	}
	
	this.destination = destination;
}

DBus.SYSTEM = dbus.DBUS_BUS_SYSTEM;
DBus.SESSION = dbus.DBUS_BUS_SESSION;

DBus.get = function(bus, destination) {
	return new DBus(bus, destination);
}

DBus.system = DBus.get.bind(undefined, DBus.SYSTEM);
DBus.session = DBus.get.bind(undefined, DBus.SESSION);


DBus.prototype.object = function(path) {
	return new DBusObject(this, path);
}

function DBusObject(bus, path) {
	EventEmitter.call(this);
	this.bus = bus;
	this.path = path;
	this.inspection = undefined;
	var self = this;

	bus.backend.registerObjectPath(path, function(message) {
		var args = [(message.interface+"."+message.member).toLowerCase()];
		Array.prototype.push.apply(args, message.arguments);
		self.emit.apply(self, args);
	})

}
util.inherits(DBusObject, EventEmitter);

DBusObject.prototype.inspect = function(callback) {

	if (this.inspection)
		return this.inspection;

	var self = this, message = dbus.methodCall(this.bus.destination, this.path, "org.freedesktop.DBus.Introspectable", "Introspect");
	
	this.bus.backend.send(message, -1, function(response) {

		DOM.parse(response.arguments[0], function(doc) {

			var res = { interfaces: { } };

			doc.querySelectorAll("interface").forEach(function(iface) {
				
				var out = { 
					name: iface.getAttribute("name"),
					methods: [ ],
					signals: [ ]
				};

				iface.querySelectorAll("method").forEach(function(method) {
					var m = {
						name: method.getAttribute("name"),
						inputs: [ ],
						outputs: [ ]
					};

					method.querySelectorAll("arg[direction=in]").forEach(function(arg) {
						m.inputs.push({ name: arg.getAttribute("name"), type: arg.getAttribute("type") })
					})

					method.querySelectorAll("arg[direction=out]").forEach(function(arg) {
						m.outputs.push({ name: arg.getAttribute("name"), type: arg.getAttribute("type") })
					})

					out.methods.push(m);
				})
				
				iface.querySelectorAll("signal").forEach(function(signal) {
					var m = {
						name: signal.getAttribute("name"),
						arguments: [ ]
					};

					signal.querySelectorAll("arg").forEach(function(arg) {
						m.arguments.push({ name: arg.getAttribute("name"), type: arg.getAttribute("type") })
					})

					out.signals.push(m);
				})

				res.interfaces[out.name] = out;

			})
			
			self.inspection = res;
			callback(res);
		});
	})
}

DBusObject.prototype.as = function(interface) {
	return new DBusProxy(this, interface);
}

function DBusProxy(object, _interface) {

	if (typeof _interface !== "string")
		throw new TypeError("Interface must be a string!");

	EventEmitter.call(this);
	var self = this;
	this.bus = object.bus;
	this.interface = _interface;
	this.object = object;

	object.inspect(function(data) {
		var interface = data.interfaces[_interface];
		
		if (typeof interface === "undefined")
			throw new Error("Unable to get interface "+_interface+"!");

		interface.methods.forEach(function(method) {
			var name = method.name;
			name = name.charAt(0).toLowerCase() + name.slice(1);

			self[name] = function() {
				var message = dbus.methodCall(self.bus.destination, object.path, interface.name, method.name), callback = arguments[arguments.length - 1];

				if (typeof callback !== "function")
					throw new TypeError("Callback must be a function!");

				message.signature = method.inputs.map(function(i) { return i.type }).join("");
				message.arguments = Array.prototype.slice.call(arguments, 0, -1);

				
				self.bus.backend.send(message, -1, (function(callback, reply) {

					switch(reply.type) {
					case dbus.DBUS_MESSAGE_TYPE_METHOD_RETURN:
						var results = !reply.arguments ? [] : Array.prototype.slice.call(reply.arguments).map(function(arg, i) {
							var signature = method.outputs[i];
							if (signature.type === "o") {

								return self.bus.object(arg);
							}
							return arg;
						});
						results.unshift(undefined);
						callback.apply(undefined, results);
						break;
					case dbus.DBUS_MESSAGE_TYPE_ERROR:
						callback(reply.error);
						break;
					case dbus.DBUS_MESSAGE_TYPE_INVALID:
					case dbus.DBUS_MESSAGE_TYPE_METHOD_CALL:
					case dbus.DBUS_MESSAGE_TYPE_SIGNAL:
						console.log("INVALID");
						break;
					}

					
				}).bind(undefined, callback))
			}
		})
		self.emit("ready");
	})
}
util.inherits(DBusProxy, EventEmitter)

DBusProxy.prototype.on = function(method, listener) {
	var self = this;
	this.object.on((this.interface+"."+method).toLowerCase(), function() {
		var args = [method];
		Array.prototype.push.apply(args, arguments);
		self.emit.apply(self, args);
	})
	return EventEmitter.prototype.on.apply(this, arguments);
}

module.exports = DBus;












