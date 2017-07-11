#include <stdlib.h>
#include <argp.h>
#include <mqueue.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

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
	"Examples:\n"
	"  mq create /myqueue\n"
	"  mq send /myqueue \"hello\" -n\n"
	"  mq info /myqueue\n"
	"  mq recv /myqueue\n"
	"  mq unlink /myqueue\n"
					 ;

static char args_doc[] =
	"create QNAME\n"
	"info QNAME\n"
	"unlink QNAME\n"
	"send QNAME MESSAGE\n"
	"recv QNAME"
	;

void usage(const struct argp *argp)
{
	argp_help(argp, stderr, ARGP_HELP_STD_HELP, PROG_NAME);
}

static struct argp_option options[] = {
	{ "verbose", 'v', 0, 0, "Produce verbose output" },
	{ "non-blocking", 'n', 0, 0, "Do not block (send, recv)" },
	{ "msgsize", 's', "SIZE", 0, "Message size in bytes (create)" },
	{ "maxmsg", 'm', "NUMBER", 0, "Maximum number of messages in queue (create)" },
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

	/* for command 'recv' */
	int blocking;

	/* for command 'send' */
	char *message;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *args = state->input;

	switch (key) {
	case 'v':
	  args->verbose = 1;
	  break;

	case 'n':
	  args->blocking = 0;
	  break;

	case 's':
	  args->msgsize = atoi(arg);
	  break;

	case 'm':
	  args->maxmsg = atoi(arg);
	  break;

	case ARGP_KEY_NO_ARGS:
	  argp_usage(state);

	case ARGP_KEY_ARG:
		if (!args->command) args->command = arg;
		else if (!args->qname) args->qname = arg;
		else if (0 == strcmp(args->command, "send") && !args->message) args->message = arg;
		else {
			/* too many arguments */
			argp_usage(state);
		}

		break;

	case ARGP_KEY_END:
		if (!args->command) argp_usage(state);
		if (!args->qname) argp_usage(state);
		if (0 == strcmp(args->command, "send") && !args->message) argp_usage(state);

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static int mqu_create(const struct arguments *args)
{
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = args->maxmsg;
	attr.mq_msgsize = args->msgsize;
	attr.mq_curmsgs = 0;

	mode_t mode = 0644;

	if (args->verbose) fprintf(stderr, "Opening mq %s (O_CREAT, O_RDWR, O_EXCL, %o)\n", args->qname, mode);

	mqd_t queue = mq_open(args->qname, O_CREAT|O_RDWR|O_EXCL, mode, &attr);

	if (-1 == queue) {
		printf("mq_open error: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

static int mqu_info(const struct arguments *args)
{
	struct mq_attr attr;
	int ret;

	if (args->verbose) fprintf(stderr, "Opening mq %s (O_RDONLY)\n", args->qname);
	mqd_t queue = mq_open(args->qname, O_RDONLY);
	if (-1 == queue) {
		printf("mq_open error: %s\n", strerror(errno));
		return 1;
	}

	ret = mq_getattr(queue, &attr);
	if (0 != ret) {
		printf("mq_getattr error: %s\n", strerror(errno));
		ret = 1;
	} else {
		printf("%s: maxmsg=%ld, msgsize=%ld, curmsgs=%ld\n",
				args->qname, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
	}

	mq_close(queue);
	return ret;
}

static int mqu_unlink(const struct arguments *args)
{
	if (args->verbose) fprintf(stderr, "Deleting mq %s\n", args->qname);
	int ret = mq_unlink(args->qname);

	if (0 != ret) {
		printf("mq_unlink error: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

static int mqu_send(const struct arguments *args)
{
	int oflag = O_WRONLY;
	if (!args->blocking) oflag |= O_NONBLOCK;

	if (args->verbose) fprintf(stderr, "Opening mq %s (O_WRONLY, %s)\n", args->qname, (oflag & O_NONBLOCK)?"O_NONBLOCK":"");
	mqd_t queue = mq_open(args->qname, oflag);
	if (-1 == queue) {
		printf("mq_open error: %s\n", strerror(errno));
		return 1;
	}

	if (args->verbose) fprintf(stderr, "Sending to mq %s: %s\n", args->qname, args->message);
	int ret = mq_send(queue, args->message, strlen(args->message)+1, 1); // keep the null terminating char
	if (0 != ret) {
		printf("mq_send error: %s\n", strerror(errno));
		ret = 1;
	}

	mq_close(queue);
	return ret;
}

static int mqu_recv(const struct arguments *args)
{
	mqd_t queue;
	int ret;
	uint8_t *buffer;
	struct mq_attr attr;
	int oflag = O_RDONLY;
	if (!args->blocking) oflag |= O_NONBLOCK;

	if (args->verbose) fprintf(stderr, "Opening mq %s (O_RDONLY, %s)\n", args->qname, (oflag & O_NONBLOCK)?"O_NONBLOCK":"");
	queue = mq_open(args->qname, oflag);
	if (-1 == queue) {
		printf("mq_open error: %s\n", strerror(errno));
		return 1;
	}

	// retrieve the message size
	ret = mq_getattr(queue, &attr);
	if (0 != ret) {
		printf("mq_getattr error: %s\n", strerror(errno));
		mq_close(queue);
		return 1;
	}

	buffer = malloc(attr.mq_msgsize);

	ret = mq_receive(queue, buffer, attr.mq_msgsize, NULL);
	if (ret >= 0) {
		/* got a message */
		printf("%s\n", buffer);
		ret = 0;
	}

	mq_close(queue);
	return ret;
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
	args.blocking = 1;
	args.message = NULL;

	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (0 == strcmp(args.command, "create")) return mqu_create(&args);
	else if (0 == strcmp(args.command, "info")) return mqu_info(&args);
	else if (0 == strcmp(args.command, "unlink")) return mqu_unlink(&args);
	else if (0 == strcmp(args.command, "send")) return mqu_send(&args);
	else if (0 == strcmp(args.command, "recv")) return mqu_recv(&args);
	else usage(&argp);
}
