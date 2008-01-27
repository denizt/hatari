#!/usr/bin/python
#
# tries to embed hatari with two different methods:
# "embed": Give SDL window into which it should reparent
#   -> SDL doesn't handle (mouse, key, expose) events
#      although according to "xev" it's window receives them!
#      Bug in SDL (not one of the originally needed features?)?
# "reparent-": Find Hatari window and reparent it into pygtk widget
#   - Needs "xwininfo" and "awk"
#   "eventbox"
#     -> PyGtk reparents it under something on rootwindow instead
#        (reparening eventbox under Hatari window works fine though...)
#   "socket"
#     -> Hatari seems to be reparented back to where it was
import os
import sys
import gtk
import time
import gobject

def usage():
    print "\nusage: %s <embed|reparent-eventbox|reparent-socket>\n" % sys.argv[0].split(os.path.sep)[-1]
    print "Opens window, runs Hatari and tries to embed it with given method\n"
    sys.exit(1)


class AppUI():
    hatari_wd = 640
    hatari_ht = 400
    
    def __init__(self, method):
        if method:
            if method == "reparent-eventbox":
                self.method = "reparent"
                widgettype = gtk.EventBox
            elif method == "reparent-socket":
                self.method = "reparent"
                widgettype = gtk.Socket
            elif method == "embed":
                self.method = "embed"
                # XEMBED socket for Hatari/SDL
                widgettype = gtk.Socket
            else:
                usage()
        else:
            usage()
        self.window = self.create_window()
        self.add_hatari_parent(self.window, widgettype)
        gobject.timeout_add(1*1000, self.timeout_cb)
        
    def create_window(self):
        window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        window.connect("destroy", self.do_quit)
        return window
    
    def do_quit(self, widget):
        if self.hatari_pid:
            os.kill(self.hatari_pid, 9)
            print "killed Hatari PID %d" % self.hatari_pid
            self.hatari_pid = 0
        gtk.main_quit()
    
    def add_hatari_parent(self, parent, widgettype):
        self.hatari_pid = 0
        widget = widgettype()
        widget.set_size_request(self.hatari_wd, self.hatari_ht)
        widget.set_events(gtk.gdk.ALL_EVENTS_MASK)
        widget.set_flags(gtk.CAN_FOCUS)
        self.hatariparent = widget
        # TODO: when running 320x200, parent could be centered to here
        parent.add(widget)
    
    def timeout_cb(self):
        self.do_hatari_method()
        return False # only once
    
    def do_hatari_method(self):
        pid = os.fork()
        if pid < 0:
            print "ERROR: fork()ing Hatari failed!"
            return
        if pid:
            # in parent
            if self.method == "reparent":
                hatari_win = self.find_hatari_window()
                if hatari_win:
                    self.reparent_hatari_window(hatari_win)
                    self.hatari_pid = pid
                else:
                    os.kill(pid, signal.SIGKILL)
                    print "killed process with PID %d" % pid
                    self.hatari_pid = 0
            else:
                # method == "embed"
                self.hatari_pid = pid
        else:
            # child runs Hatari
            args = ("hatari", "-m", "-z", "2")
            os.execvpe("hatari", args, self.get_hatari_env())

    def get_hatari_env(self):
        if self.method == "reparent":
            return os.environ
        # tell SDL to use (embed itself inside) given widget's window
        win_id = self.hatariparent.get_id()
        env = os.environ
        env["SDL_WINDOWID"] = str(win_id)
        return env
    
    def find_hatari_window(self):
        # find hatari window by its WM class string and reparent it
        cmd = """xwininfo -root -tree|awk '/"hatari" "hatari"/{print $1}'"""
        counter = 0
        while counter < 8:
            pipe = os.popen(cmd)
            windows = []
            for line in pipe.readlines():
                windows.append(int(line, 16))
            try:
                pipe.close()
            except IOError:
                # handle child process exiting silently
                pass
            if not windows:
                counter += 1
                print "WARNING: no Hatari window found yet, retrying..."
                time.sleep(1)
                continue
            if len(windows) > 1:
                print "WARNING: multiple Hatari windows, picking first one..."
            return windows[0]
        print "ERROR: no windows with the 'hatari' WM class found"
        return None
    
    def reparent_hatari_window(self, hatari_win):
        window = gtk.gdk.window_foreign_new(hatari_win)
        if not window:
            print "ERROR: Hatari window (ID: 0x%x) reparenting failed!" % hatari_win
            return False
        if not self.hatariparent.window:
            print "ERROR: where hatariparent disappeared?"
            return False
        print "Found Hatari window ID: 0x%x, reparenting..." % hatari_win
        print "...to container window ID: 0x%x" % self.hatariparent.window.xid
        window.reparent(self.hatariparent.window, 0, 0)
        #window.reparent(self.hatariparent.get_toplevel().window, 0, 0)
        #window.reparent(self.hatariparent.get_root_window(), 0, 0)
        #window.show()
        #window.raise_()
        # If python would destroy the Gtk widget when it goes out of scope,
        # the foreign window widget destructor would delete Hatari window.
        # So, keep a reference
        #self.hatariwindow = window
        return True

    def run(self):
        self.window.show_all()
        gtk.main()


if len(sys.argv) != 2:
    usage()
app = AppUI(sys.argv[1])
app.run()
