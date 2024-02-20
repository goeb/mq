
# mq - a command line tool to use Posix Message Queues

```
Usage: mq [OPTION...] create QNAME
  or:  mq [OPTION...] info QNAME
  or:  mq [OPTION...] unlink QNAME
  or:  mq [OPTION...] send QNAME [MESSAGE]
  or:  mq [OPTION...] recv QNAME
A command line tool to use Posix Message Queues from the shell

 Options:
  -t, --timestamp            Print a timestamp before lines of data
  -v, --verbose              Produce verbose output

 Options for create:
  -m, --maxmsg=NUMBER        Maximum number of messages in queue
  -s, --msgsize=SIZE         Message size in bytes

 Options for recv:
  -f, --follow               Print messages as they are received

 Options for send:
  -p, --priority=PRIO        Use priority PRIO, PRIO >= 0

 Options for send, recv:
  -n, --non-blocking         Do not block (send, recv)

  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

If the MESSAGE parameter is not provided for the send command, the message data will be read from standard input.

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
  echo "hello" | mq send /myqueue
  mq info /myqueue
  mq recv /myqueue
  mq unlink /myqueue
```


