/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "config.h"
#include "fuse_lowlevel.h"
#include "fuse_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

static struct fuse_session *fuse_instance;

/*! [doxygen_exit_handler] */
static void exit_handler(int sig)
{
	(void) sig;
	if (fuse_instance)
		fuse_session_exit(fuse_instance);
}
/*! [doxygen_exit_handler] */

static void stats_handler(int sig)
{
	(void) sig;
	int /*fd,*/ i, j;
	FILE *fp = NULL;
	struct fuse_session *se;
	char *statsDir = NULL, *statsFile = NULL;

	se = fuse_instance;
	statsDir = fuse_session_statsDir(se);
	if (!statsDir) {
		printf("No Stats Directory to copy the statistics\n");
		return ;
	}
	statsFile = (char *)malloc(4096 * sizeof(char));
	statsFile[0] = '\0';
	strcpy(statsFile, statsDir);
	strcat(statsFile, "/user_stats.txt");

	pthread_spin_lock(&se->lock);
	fp = fopen(statsFile , "w" );
	if (fp) {
		for (i = 1; i < 46; i++) {
			for (j = 0; j < 33; j++)
				fprintf(fp, "%llu ", se->processing[i][j]);
			fprintf(fp, "\n");
		}
	} else {
		perror("Failed to open User Stats File");
	}
	if (statsFile)
		free(statsFile);
	if (fp)
		fclose(fp);
	/* print the argument values to screen (remove soon after) */
	printf("Print mount options\n");
	printf("allow_root : %d\n", se->f->allow_root);
	printf("max_write : %u\n", se->f->conn.max_write);
	printf("max_readahead : %u\n", se->f->conn.max_readahead);
	printf("max_background : %u\n", se->f->conn.max_background);
	printf("congestion_threshold : %u\n", se->f->conn.congestion_threshold);
	printf("async_read : %u\n", se->f->conn.async_read);
	printf("sync_read : %u\n", ~(se->f->conn.async_read));
	printf("atomic_o_trunc : %d\n", se->f->atomic_o_trunc);
	printf("no_remote_lock : %d\n", (se->f->no_remote_flock)&(se->f->no_remote_posix_lock));
	printf("no_remote_flock : %d\n", se->f->no_remote_flock);
	printf("no_remote_posix_lock : %d\n", se->f->no_remote_posix_lock);
	printf("big_writes : %d\n", se->f->big_writes);
	printf("splice_write : %d\n", se->f->splice_write);
	printf("splice_move : %d\n", se->f->splice_move);
	printf("splice_read : %d\n", se->f->splice_read);
	//printf("no_splice_move : %d\n", se->f->no_splice_move);
	//printf("no_splice_read : %d\n", se->f->no_splice_read);
	//printf("no_splice_write : %d\n", se->f->no_splice_write);
	printf("auto_inval_data : %d\n", se->f->auto_inval_data);
	//printf("no_auto_inval_data : %d\n", se->f->no_auto_inval_data);
	printf("no_readdirplus : %d\n", se->f->no_readdirplus);
	printf("no_readdirplus_auto : %d\n", se->f->no_readdirplus_auto);
	printf("async_dio : %d\n", se->f->async_dio);
	//printf("no_async_dio : %d\n", se->f->no_async_dio);
	printf("writeback_cache : %d\n", se->f->writeback_cache);
	//printf("no_writeback_cache : %d\n", se->f->no_writeback_cache);
	printf("time_gran : %u\n", se->f->conn.time_gran);
	printf("clone_fd : %d\n", se->f->clone_fd);

	pthread_spin_unlock(&se->lock);
}

static int set_one_signal_handler(int sig, void (*handler)(int), int remove)
{
	struct sigaction sa;
	struct sigaction old_sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = remove ? SIG_DFL : handler;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;

	if (sigaction(sig, NULL, &old_sa) == -1) {
		perror("fuse: cannot get old signal handler");
		return -1;
	}

	if (old_sa.sa_handler == (remove ? handler : SIG_DFL) &&
	    sigaction(sig, &sa, NULL) == -1) {
		perror("fuse: cannot set signal handler");
		return -1;
	}
	return 0;
}

int fuse_set_signal_handlers(struct fuse_session *se)
{
	if (set_one_signal_handler(SIGHUP, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGINT, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGTERM, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGPIPE, SIG_IGN, 0) == -1 ||
	    set_one_signal_handler(SIGUSR1, stats_handler, 0) == -1 )
		return -1;
	printf("Sucessfully registered signal Handlers : %d \n", getpid());
	fuse_instance = se;
	return 0;
}

void fuse_remove_signal_handlers(struct fuse_session *se)
{
	if (fuse_instance != se)
		fprintf(stderr,
			"fuse: fuse_remove_signal_handlers: unknown session\n");
	else
		fuse_instance = NULL;

	set_one_signal_handler(SIGHUP, exit_handler, 1);
	set_one_signal_handler(SIGINT, exit_handler, 1);
	set_one_signal_handler(SIGTERM, exit_handler, 1);
	set_one_signal_handler(SIGPIPE, SIG_IGN, 1);
	set_one_signal_handler(SIGUSR1, stats_handler, 1);
}
