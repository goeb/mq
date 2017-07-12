
# mq - a command line tool to use Posix Message Queues

```
Usage: mq [OPTION...] create QNAME
  or:  mq [OPTION...] info QNAME
  or:  mq [OPTION...] unlink QNAME
  or:  mq [OPTION...] send QNAME MESSAGE
  or:  mq [OPTION...] recv QNAME
A command line tool to use Posix Message Queues from the shell

  -f, --follow               Print messages as they are received (recv)
  -m, --maxmsg=NUMBER        Maximum number of messages in queue (create)
  -n, --non-blocking         Do not block (send, recv)
  -p, --priority=PRIO        Use priority PRIO, PRIO >= 0 (send)
  -s, --msgsize=SIZE         Message size in bytes (create)
  -t, --timestamp            Print a timestamp (send, recv)
  -v, --verbose              Produce verbose output
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Commands:
  create    Create a POSIX message queue
  info      Print information about an existing message queue
  unlink    Delete a message queue
  send      Send a message to a message queue
  recv      Receive and print a message from a message queue

Examples:
  mq create /myqueue
  mq send /myqueue "hello" -n
  mq info /myqueue
  mq recv /myqueue
  mq unlink /myqueue
```

# TODO

- `mq recv /myqueue --timeout 1000`
- in the help, group command-specific options by command
