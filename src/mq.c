#include <stdlib.h>
#include <argp.h>
#include <mqueue.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>

#define PROG_NAME "mq"
const char *argp_program_version = PROG_NAME " 1.0";

static char doc[] = 
	"A command line tool to use Posix Message Queues from the shell"
	"\vCommands:\n"
	"  create    Create a POSIX message queue\n"
	"  info      Print information about an existing message queue\n"
	"  unlink    Delete a message queue\n"
	"  send      Send a message to a message queue\n"
	"  recv      Receive and print a message from a message queue\n"
	"\n"
	"Delimiters:\n"
	"  n         new line (LF) [default]\n"
	"  z         zero (NUL)\n"
	"  x         no delimiter\n"
	"\n"
	"\n"
	"Examples:\n"
	"  mq create /myqueue\n"
	"  mq send /myqueue \"hello\" -n\n"
	"  mq info /myqueue\n"
	"  mq recv /myqueue\n"
	"  mq unlink /myqueue\n"
	"\n"
	;

static char args_doc[] =
	"create QNAME\n"
	"info QNAME\n"
	"unlink QNAME\n"
	"send QNAME MESSAGE\n"
	"recv QNAME"
	;

static void print_hexa(const uint8_t *buffer, size_t size)
{
	for (size_t i=0; i<size; i++) {
		if (i > 0) printf(" ");
		printf("%02x", buffer[i]);
	}
}

#define LOG_ERR(...) \
		do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)

#define LOG_VERBOSE(_args, ...) \
		do { if (_args->verbose) { \
			printf("%s ", get_timestamp()); \
			printf(__VA_ARGS__); \
			printf("\n"); } \
		} while (0)

#define LOG_VERBOSE_HEXA(_args, buffer, size) \
		do { if (_args->verbose) { \
			printf("%s ", get_timestamp()); \
			print_hexa(buffer, size); /* TODO */ \
			printf("\n"); } \
		} while (0)

static char *get_timestamp(void)
{
	static char buffer[256];
	struct tm date;
	struct timeval tv;
	gettimeofday(&tv, 0);
	localtime_r(&tv.tv_sec, &date);
	int milliseconds = tv.tv_usec / 1000;
	snprintf(buffer, sizeof(buffer), "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%03d",
			date.tm_year + 1900,
			date.tm_mon + 1,
			date.tm_mday,
			date.tm_hour,
			date.tm_min,
			date.tm_sec,
			milliseconds);
	return buffer;
}

static void usage(const struct argp *argp)
{
	argp_help(argp, stderr, ARGP_HELP_STD_HELP, (char *)PROG_NAME);
}

static struct argp_option options[] = {
	{ 0, 0, 0, 0, "Options:" },
	{ "verbose", 'v', 0, 0, "Produce verbose output" },
	{ "timestamp", 't', 0, 0, "Print a timestamp before lines of data" },
	{ 0, 0, 0, 0, "Options for create:" },
	{ "msgsize", 's', "SIZE", 0, "Message size in bytes" },
	{ "maxmsg", 'm', "NUMBER", 0, "Maximum number of messages in queue" },
	{ 0, 0, 0, 0, "Options for recv:" },
	{ "follow", 'f', 0, 0, "Print messages as they are received" },
	{ 0, 0, 0, 0, "Options for send:" },
	{ "priority", 'p', "PRIO", 0, "Use priority PRIO, PRIO >= 0" },
	{ 0, 0, 0, 0, "Options for send, recv:" },
	{ "non-blocking", 'n', 0, 0, "Do not block (send, recv)" },
	{ "delimiter", 'd', "CHAR", 0, "Character to delimit the end of messages (see delimiters)" },
	{ 0 }
};

struct arguments
{
	int verbose;
	char *command;
	char *qname; /* name of the mq (should start with '/') */

	/* for command 'create' */
	int maxmsg; /* max number of message */
	int msgsize; /* size of a message */

	char delimiter;

	int timestamp;
	/* for command 'recv' */
	int blocking;
	int follow;

	/* for command 'send' */
	char *message;
	size_t msglen;
	int priority;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;

	switch (key) {
	case 'v': args->verbose = 1; break;
	case 'n': args->blocking = 0; break;
	case 't': args->timestamp = 1; break;
	case 'f': args->follow = 1; break;
	case 's': args->msgsize = atoi(arg); break;
	case 'm': args->maxmsg = atoi(arg); break;
	case 'p': args->priority = atoi(arg); break;
	case 'd':
		if (0 == strcmp("n", arg)) args->delimiter = '\n';
		else if (0 == strcmp("z", arg)) args->delimiter = '\0';
		else if (0 == strcmp("x", arg)) args->delimiter = 'x';
		else {
			LOG_ERR("Invalid delimiter speicifer '%s' (use 'n' or 'z' or 'x')", arg);
			return ARGP_ERR_UNKNOWN;
		}
		break;

	case ARGP_KEY_NO_ARGS:
		argp_usage(state);
		break;

	case ARGP_KEY_ARG:
		if (!args->command) args->command = arg;
		else if (!args->qname) args->qname = arg;
		else if (0 == strcmp(args->command, "send") && !args->message) {
			args->message = arg;
			args->msglen = strlen(arg);
		} else {
			/* too many arguments */
			argp_usage(state);
		}
		break;

	case ARGP_KEY_END:
		if (!args->command) argp_usage(state);
		if (!args->qname) argp_usage(state);
		if (0 == strcmp(args->command, "send") && !args->message) argp_usage(state);
		return ARGP_ERR_UNKNOWN;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static int cmd_create(const struct arguments *args)
{
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = args->maxmsg;
	attr.mq_msgsize = args->msgsize;
	attr.mq_curmsgs = 0;

	mode_t mode = 0644;

	LOG_VERBOSE(args, "Opening mq %s (O_CREAT, O_RDWR, O_EXCL, %o)", args->qname, mode);

	mqd_t queue = mq_open(args->qname, O_CREAT|O_RDWR|O_EXCL, mode, &attr);

	if (-1 == queue) {
		LOG_ERR("mq_open error: %s", strerror(errno));
		return 1;
	}
	return 0;
}

static int cmd_info(const struct arguments *args)
{
	struct mq_attr attr;
	int ret;

	LOG_VERBOSE(args, "Opening mq %s (O_RDONLY)", args->qname);
	mqd_t queue = mq_open(args->qname, O_RDONLY);
	if (-1 == queue) {
		LOG_ERR("mq_open error: %s", strerror(errno));
		return 1;
	}

	ret = mq_getattr(queue, &attr);
	if (0 != ret) {
		LOG_ERR("mq_getattr error: %s", strerror(errno));
		ret = 1;
	} else {
		printf("%s: maxmsg=%ld, msgsize=%ld, curmsgs=%ld\n",
		       args->qname, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
	}

	mq_close(queue);
	return ret;
}

static int cmd_unlink(const struct arguments *args)
{
	LOG_VERBOSE(args, "Deleting mq %s", args->qname);
	int ret = mq_unlink(args->qname);

	if (0 != ret) {
		LOG_ERR("mq_unlink error: %s", strerror(errno));
		return 1;
	}
	return 0;
}

static int cmd_send(const struct arguments *args)
{
	int oflag = O_WRONLY;
	if (!args->blocking) oflag |= O_NONBLOCK;

	LOG_VERBOSE(args, "Opening mq %s (O_WRONLY%s)", args->qname, (oflag & O_NONBLOCK)?", O_NONBLOCK":"");
	mqd_t queue = mq_open(args->qname, oflag);
	if (-1 == queue) {
		LOG_ERR("mq_open error: %s", strerror(errno));
		return 1;
	}

	LOG_VERBOSE_HEXA(args, (const uint8_t *)args->message, args->msglen);

	/* Send */
	int ret = mq_send(queue, args->message, args->msglen, args->priority);
	if (0 != ret) {
		LOG_ERR("mq_send error: %s", strerror(errno));
		ret = 1;
	}

	mq_close(queue);
	return ret;
}

static mqd_t mqu_open_ro(const struct arguments *args)
{
	mqd_t queue;
	int oflag = O_RDONLY;
	if (!args->blocking) oflag |= O_NONBLOCK;

	LOG_VERBOSE(args, "Opening mq %s (O_RDONLY%s)",
	            args->qname, (oflag & O_NONBLOCK)?", O_NONBLOCK":"");

	queue = mq_open(args->qname, oflag);
	if (-1 == queue) LOG_ERR("mq_open error: %s", strerror(errno));

	return queue;
}

// Precondition: sz is greater than zero.
static void write_msg_with_delimiter(const struct arguments *args, uint8_t *buffer, size_t sz) {
	size_t messageRemainingBytes = sz;
        do {
		ssize_t messageWrittenBytes = write(1, buffer, messageRemainingBytes);
        	if(messageWrittenBytes == -1) {
			LOG_ERR("mq_receive error writing message: %s", strerror(errno));
        		exit(1);
        	} else {
        		messageRemainingBytes = messageRemainingBytes - (size_t)messageWrittenBytes;
        	}
        } while (messageRemainingBytes > 0);
        if(args->delimiter != 'x') {
        	ssize_t delimiterWrittenBytes = write(1, &args->delimiter, 1);
        	if(delimiterWrittenBytes == -1) {
        		LOG_ERR("mq_receive error writing delimiter: %s", strerror(errno));
        		exit(1);
        	} else if(delimiterWrittenBytes != 1) {
        		fprintf(stderr, "mq_receive error: Expected single byte to be written\n");
        		exit(1);
        	}
        }
}

static int cmd_recv(const struct arguments *args)
{
	mqd_t queue;
	int ret;
	uint8_t *buffer;
	struct mq_attr attr;

	queue = mqu_open_ro(args);
	if (-1 == queue) return 1;

	// retrieve the message size
	ret = mq_getattr(queue, &attr);
	if (0 != ret) {
		mq_close(queue);
		return 1;
	}

	buffer = malloc(attr.mq_msgsize);

	ssize_t n = mq_receive(queue, (void*)buffer, attr.mq_msgsize, NULL);
	if (n >= 0) {
		/* got a message */
		LOG_VERBOSE_HEXA(args, buffer, n);
                write_msg_with_delimiter(args,buffer,(size_t)n);
		ret = 0;
	} else {
		LOG_ERR("mq_receive error: %s", strerror(errno));
		ret = 1;
	}

	mq_close(queue);
	return ret;
}

static int cmd_recv_follow(const struct arguments *args)
{
	mqd_t queue;
	int ret;
	uint8_t *buffer;
	struct mq_attr attr;
	struct pollfd ufds[1];

	queue = mqu_open_ro(args);
	if (-1 == queue) return 1;

	// retrieve the message size
	ret = mq_getattr(queue, &attr);
	if (0 != ret) {
		mq_close(queue);
		return 1;
	}

	buffer = malloc(attr.mq_msgsize);

	ufds[0].fd = queue;
	ufds[0].events = POLLIN;

	ret = 1;
	while (1) {
		int rv = poll(ufds, 1, -1); // no timeout
		if (rv == -1) {
			LOG_ERR("poll error: %s", strerror(errno));
		} else if (1 == rv) {
			if (ufds[0].revents & POLLIN) {
				// receive the message
				ssize_t n = mq_receive(queue, (void*)buffer, attr.mq_msgsize, NULL);
				if (n >= 0) {
					/* got a message */
					LOG_VERBOSE_HEXA(args, buffer, n);
                			write_msg_with_delimiter(args,buffer,(size_t)n);
				} else {
					LOG_ERR("mq_receive error: %s", strerror(errno));
					break;
				}

			} else {
				LOG_ERR("poll revents != POLLIN (%x)", ufds[0].revents);
				break;
			}
		} else {
			LOG_ERR("poll error(2): rv=%d", rv);
		}
	}
	mq_close(queue);
	return 1;

}

int main(int argc, char **argv)
{
	struct arguments args;
	struct argp argp = { options, parse_opt, args_doc, doc };

	/* Default values */
	args.verbose = 0;
	args.command = NULL;
	args.qname = NULL;
	args.maxmsg = 10;
	args.msgsize = 1024;
	args.timestamp = 0;
	args.blocking = 1;
	args.follow = 0;
	args.message = NULL;
	args.msglen = 0;
	args.priority = 0;
	args.delimiter = '\n';

	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (0 == strcmp(args.command, "create")) return cmd_create(&args);
	else if (0 == strcmp(args.command, "info")) return cmd_info(&args);
	else if (0 == strcmp(args.command, "unlink")) return cmd_unlink(&args);
	else if (0 == strcmp(args.command, "send")) return cmd_send(&args);
	else if (0 == strcmp(args.command, "recv")) {
	   if (args.follow) return cmd_recv_follow(&args);
	   else return cmd_recv(&args);
	}
	else usage(&argp);
}
