#!/usr/bin/env python
#
# Hatari console:
# Allows using Hatari shortcuts & debugger, changing paths, toggling
# devices and changing Hatari command line options (even for things you
# cannot change from the UI) from the console while Hatari is running.
#
# Copyright (C) 2008-2010 by Eero Tamminen <eerot at berlios>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import os
import sys
import time
import signal
import socket
import readline

# Atari scancodes for special keys, needed when wanting to
# output something else than alphanumberic keys to Hatari
# (when using keypress/down/up commands)
class Scancode:
    Tab = "15"
    Return = "28"
    Enter = "114"
    Space = "57"
    Delete = "83"
    Backspace = "14"
    Escape = "0x1"
    Control = "29"
    Alternate = "56"
    LeftShift = "42"
    RightShift = "54"
    CapsLock = "53"
    Insert = "82"
    Home = "71"
    Help = "98"
    Undo = "97"
    CursorUp = "72"
    CursorDown = "80"
    CursorLeft = "75"
    CursorRight = "77"


# running Hatari instance
class Hatari:
    controlpath = "/tmp/hatari-console-" + str(os.getpid()) + ".socket"
    hataribin = "hatari"

    def __init__(self, args = None):
        # collect hatari process zombies without waitpid()
        signal.signal(signal.SIGCHLD, signal.SIG_IGN)
        self._assert_hatari_compatibility()
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        if os.path.exists(self.controlpath):
            os.unlink(self.controlpath)
        self.server.bind(self.controlpath)
        self.server.listen(1)
        self.control = None
        self.paused = False
        self.interval = 0.2
        self.pid = 0
        if not self.run_hatari(args):
            print "ERROR: failed to run Hatari"
            sys.exit(1)
        self.verbose = False

    def _assert_hatari_compatibility(self):
        for line in os.popen(self.hataribin + " -h").readlines():
            if line.find("--control-socket") >= 0:
                return
        print "ERROR: Hatari not found or it doesn't support the required --control-socket option!"
        sys.exit(-1)
        
    def is_running(self):
        if not self.pid:
            return False
        try:
            os.waitpid(self.pid, os.WNOHANG)
        except OSError, value:
            print "Hatari PID %d had exited in the meanwhile:\n\t%s" % (self.pid, value)
            self.pid = 0
            return False
        return True
    
    def run_hatari(self, args):
        if self.control:
            print "ERROR: Hatari is already running, stop it first"
            return
        pid = os.fork()
        if pid < 0:
            print "ERROR: fork()ing Hatari failed!"
            return
        if pid:
            # in parent
            self.pid = pid
            print "WAIT hatari to connect to control socket...",
            (self.control, addr) = self.server.accept()
            print "connected!"
            return self.control
        else:
            # child runs Hatari
            allargs = [self.hataribin, "--control-socket", self.controlpath] + args
            print "RUN:", allargs
            os.execvp(self.hataribin, allargs)

    def send_message(self, msg, fast = False):
        if self.control:
            if self.verbose:
                print "-> '%s'" % msg
            self.control.sendall(msg + "\n")
            # KLUDGE: wait so that Hatari output comes before next prompt
            if fast:
                interval = self.interval/2
            else:
                interval = self.interval
            time.sleep(interval)
            return True
        else:
            print "ERROR: no Hatari (control socket)"
            return False
        
    def change_option(self, option):
        return self.send_message("hatari-option %s" % option)

    def trigger_shortcut(self, shortcut):
        return self.send_message("hatari-shortcut %s" % shortcut)

    def send_string(self, text):
        print "string:", text
        for item in text:
            if item == ' ':
                # white space gets stripped, use scancode instead
                item = "57"
            if not self.send_message("hatari-event keypress %s" % item, True):
                return False
        return True

    def insert_event(self, event):
        if event.startswith("text "):
            cmd, value = event.split(None, 1)
            if value:
                return self.send_string(value)
        return self.send_message("hatari-event %s" % event, True)

    def debug_command(self, cmd):
        return self.send_message("hatari-debug %s" % cmd)
    
    def change_path(self, path):
        return self.send_message("hatari-path %s" % path)
    
    def toggle_device(self, device):
        return self.send_message("hatari-toggle %s" % device)

    def toggle_pause(self):
        self.paused = not self.paused
        if self.paused:
            return self.send_message("hatari-stop")
        else:
            return self.send_message("hatari-cont")

    def toggle_verbose(self):
        self.verbose = not self.verbose
        print "debug output", self.verbose

    def kill_hatari(self):
        if self.pid:
            os.kill(self.pid, signal.SIGKILL)
            print "killed hatari with PID %d" % self.pid
            self.control = None
            self.pid = 0


# command line parsing with readline
class CommandInput:
    prompt = "hatari-command: "
    historysize = 99
    
    def __init__(self, commands):
        readline.set_history_length(self.historysize)
        readline.parse_and_bind("tab: complete")
        readline.set_completer_delims(" \t\r\n")
        readline.set_completer(self.complete)
        self.commands = commands
    
    def complete(self, text, state):
        idx = 0
        #print "text: '%s', state '%d'" % (text, state)
        for cmd in self.commands:
            if cmd.startswith(text):
                idx += 1
                if idx > state:
                    return cmd
    
    def loop(self):
        try:
            rawline = raw_input(self.prompt)
            return rawline
        except EOFError:
            return ""


class Tokens:
    # update with: hatari -h|grep -- --|sed 's/^ *\(--[^ ]*\).*$/    "\1",/'
    option_tokens = [
    "--help",
    "--version",
    "--confirm-quit",
    "--configfile",
    "--fast-forward",
    "--mono",
    "--monitor",
    "--fullscreen",
    "--window",
    "--grab",
    "--zoom",
    "--max-width",
    "--max-height",
    "--aspect",
    "--borders",
    "--frameskips",
    "--statusbar",
    "--drive-led",
    "--spec512",
    "--bpp",
    "--vdi",
    "--vdi-planes",
    "--vdi-width",
    "--vdi-height",
    "--avirecord",
    "--avi-vcodec",
    "--avi-fps",
    "--avi-crop",
    "--avi-file",
    "--joy0",
    "--joy1",
    "--joy2",
    "--joy3",
    "--joy4",
    "--joy5",
    "--joystick",
    "--printer",
    "--midi-in",
    "--midi-out",
    "--rs232-in",
    "--rs232-out",
    "--disk-a",
    "--disk-b",
    "--slowfdc",
    "--protect-floppy",
    "--protect-hd",
    "--harddrive",
    "--acsi",
    "--ide-master",
    "--ide-slave",
    "--memsize",
    "--tos",
    "--cartridge",
    "--memstate",
    "--cpulevel",
    "--cpuclock",
    "--compatible",
    "--machine",
    "--blitter",
    "--timer-d",
    "--dsp",
    "--sound",
    "--keymap",
    "--debug",
    "--bios-intercept",
    "--trace",
    "--trace-file",
    "--parse",
    "--saveconfig",
    "--log-file",
    "--log-level",
    "--alert-level",
    "--run-vbls"
    ]
    shortcut_tokens = [
    "mousegrab",
    "coldreset",
    "warmreset",
    "screenshot",
    "bosskey",
    "recanim",
    "recsound",
    "savemem"
    ]
    event_tokens = [
    "doubleclick",
    "rightdown",
    "rightup",
    "keypress",
    "keydown",
    "keyup",
    "text"	# simulated with keypresses
    ]
    device_tokens = [
    "printer",
    "rs232",
    "midi",
    ]
    path_tokens = [
    "memauto",
    "memsave",
    "midiout",
    "printout",
    "soundout",
    "rs232in",
    "rs232out"
    ]
    # use the long variants of the commands for clarity
    debugger_tokens = [
    "address",
    "breakpoint",
    "cd",
    "cont",
    "cpureg",
    "disasm",
    "dspaddress",
    "dspbreak",
    "dspcont",
    "dspdisasm",
    "dspmemdump",
    "dspreg",
    "dspsymbols",
    "evaluate",
    "exec",
    "help",
    "info",
    "loadbin",
    "lock",
    "logfile",
    "memdump",
    "memwrite",
    "parse",
    "savebin",
    "setopt",
    "stateload",
    "statesave",
    "symbols",
    "trace"
    ]

    def __init__(self, hatari):
        self.process_tokens = {
            "kill": hatari.kill_hatari,
            "pause": hatari.toggle_pause
        }
        self.script_tokens = {
            "script": self.do_script,
            "sleep": self.do_sleep
        }
        self.help_tokens = {
            "usage": self.show_help,
            "verbose": hatari.toggle_verbose
        }
        self.hatari = hatari

    def get_tokens(self):
        tokens = []
        for items in [self.option_tokens, self.shortcut_tokens,
            self.event_tokens, self.debugger_tokens, self.device_tokens,
            self.path_tokens, self.process_tokens.keys()]:
            for token in items:
                if token in tokens:
                    print "ERROR: token '%s' already in tokens" % token
                    sys.exit(1)
            tokens += items
        return tokens

    def show_help(self):
        print """
Hatari-console help
-------------------

Hatari-console allows you to control Hatari through its control socket
from the provided console prompt, while Hatari is running.  All control
commands support TAB completion on their names and options.

The supported control facilities are:"""
        self.list_items("Command line options", self.option_tokens)
        self.list_items("Keyboard shortcuts", self.shortcut_tokens)
        self.list_items("Event invocation", self.event_tokens)
        self.list_items("Device toggling", self.device_tokens)
        self.list_items("Path setting", self.path_tokens)
        self.list_items("Debugger commands", self.debugger_tokens)
        print """
"pause" toggles Hatari paused state on/off.
"kill" will terminate Hatari.

"script" command reads commands from the given file.
"sleep" command can be used in script to wait given number of seconds.
"verbose" command toggles commands debug output on/off.

For command line options you can get further help with "--help"
and for debugger commands with "help".  Some of the other facilities
give help when you give them invalid input.
"""

    def list_items(self, title, items):
        print "\n%s:" % title
        for item in items:
            print "*", item

    def do_sleep(self, line):
        items = line.split()[1:]
        try:
            secs = int(items[0])
        except:
            secs = 0
        if secs > 0:
            print "Sleeping for %d secs..." % secs
            time.sleep(secs)
        else:
            print "usage: sleep <seconds>"

    def do_script(self, line):
        try:
            filename = line.split()[1]
            f = open(filename)
        except:
            print "usage: script <filename>"
            return

        for line in f.readlines():
            line = line.strip()
            if not line or line[0] == '#':
                continue
            print ">", line
            self.process_command(line)

    def process_command(self, line):
        if not self.hatari.is_running():
            print "Exiting as there's no Hatari (anymore)..."
            sys.exit(0)
        if not line:
            return

        first = line.split()[0]
        # multiple items
        if first in self.event_tokens:
            self.hatari.insert_event(line)
        elif first in self.debugger_tokens:
            self.hatari.debug_command(line)
        elif first in self.option_tokens:
            self.hatari.change_option(line)
        elif first in self.path_tokens:
            self.hatari.change_path(line)
        elif first in self.script_tokens:
            self.script_tokens[first](line)
        # single item
        elif line in self.device_tokens:
            self.hatari.toggle_device(line)
        elif line in self.shortcut_tokens:
            self.hatari.trigger_shortcut(line)
        elif line in self.process_tokens:
            self.process_tokens[line]()
        elif line in self.help_tokens:
            self.help_tokens[line]()
        else:
            print "ERROR: unknown hatari-console command:", line


class Main:
    def __init__(self):
        args, self.file, self.exit = self.parse_args(sys.argv)
        hatari = Hatari(args)
        self.tokens = Tokens(hatari)
        self.command = CommandInput(self.tokens.get_tokens())

    def parse_args(self, args):
        if "-h" in args or "--help" in args:
            self.usage()

        file = []
        exit = False
        if "--" not in args:
            return (args[1:], file, exit)
        
        for arg in args:
            if arg == "--":
                return (args[args.index("--")+1:], file, exit)
            if arg == "--exit":
                exit = True
                continue
            if os.path.exists(arg):
                file = arg
            else:
                self.usage("file '%s' not found" % arg)

    def usage(self, msg=None):
        name = os.path.basename(sys.argv[0])
        print "\n%s" % name
        print "=" * len(name)
        print """
Usage: %s [<console options/args> --] [<hatari options>]

Hatari console options/args:
\t<file>\t\tread commands from given file
\t--exit\t\texit after executing the commands in the file
\t-h, --help\t\tthis help

Except for help, console options/args will be interpreted
only if '--' is given as one of the arguments.  Otherwise
all arguments are given to Hatari.

For example:
    %s --monitor mono test.prg
    %s commands.txt -- --monitor mono
    %s commands.txt --exit --
""" % (name, name, name, name)
        if msg:
            print "ERROR: %s!\n" % msg
        sys.exit(1)

    def loop(self):
        print """
*********************************************************
* To see available commands, use the TAB key or 'usage' *
*********************************************************
"""
        if self.file:
            self.script(self.file)
            if self.exit:
                sys.exit(0)

        while 1:
            line = self.command.loop().strip()
            self.tokens.process_command(line)

    def script(self, filename):
        self.tokens.do_script("script " + filename)

    def run(self, line):
        "helper method for scripts using hatari-console"
        self.tokens.process_command(line)


if __name__ == "__main__":
    Main().loop()