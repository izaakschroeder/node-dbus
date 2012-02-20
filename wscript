def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  conf.check_cfg(package='dbus-1', uselib_store='DBUS', args='--cflags --libs', mandatory=True)


def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.uselib = [ "DBUS" ]
  obj.target = "dbus"
  obj.source = "dbus.cc"
  