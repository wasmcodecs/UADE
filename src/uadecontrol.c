
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <uae.h>

#include "uadecontrol.h"
#include "strlrep.h"

#define INPUT_BUF_SIZE (4096)

unsigned int uade_inputbytes = 0;
static char uade_inputbuffer[INPUT_BUF_SIZE];


int uade_input_fd = 0; /* stdin */
int uade_output_fd = 1; /* stdout */


static int get_more(unsigned int bytes)
{
  ssize_t s;
  int ret;
  struct pollfd pollfd;
  pollfd.fd = 0;
  pollfd.events = POLLIN;

  while (uade_inputbytes < bytes) {
    ret = poll(&pollfd, 1, -1);
    if (ret == 0)
      continue;
    if (ret < 0) {
      if (errno == EINTR)
	continue;
      assert(0);
    }
    s = read(uade_input_fd, &uade_inputbuffer[uade_inputbytes], sizeof(uade_inputbuffer) - uade_inputbytes);
    if (s < 0) {
      if (errno == EAGAIN || errno == EINTR)
	continue;
      assert(0);
    } else if (s == 0) {
      fprintf(stderr, "uade: no more input. exiting.\n");
      quit_program = 1;
      return 0;
    }
    uade_inputbytes += s;
  }
  return 1;
}


static void uade_copy_from_inputbuffer(void *dst, int bytes)
{
  if (uade_inputbytes < bytes) {
    fprintf(stderr, "not enough bytes in input buffer\n");
    exit(-1);
  }
  memcpy(dst, uade_inputbuffer, bytes);
  memmove(uade_inputbuffer, &uade_inputbuffer[bytes], uade_inputbytes - bytes);
  uade_inputbytes -= bytes;
}


int uade_get_command(struct uade_control *uc, size_t maxbytes)
{
  size_t fullsize;

  assert(sizeof(*uc) == 8);

  if (uade_inputbytes < sizeof(*uc)) {
    if ((get_more(sizeof(*uc)) == 0))
      return 0;
  }
  uade_copy_from_inputbuffer(uc, sizeof(*uc));

  if (uc->command <= UADE_COMMAND_FIRST || uc->command >= UADE_COMMAND_LAST) {
    fprintf(stderr, "illegal command number: %d\n", uc->command);
    return 0;
  }

  fullsize = uc->size + sizeof(*uc);
  if (fullsize > INPUT_BUF_SIZE) {
    fprintf(stderr, "too big a command size: %u\n", fullsize);
    return 0;
  }
  if (fullsize > maxbytes) {
    fprintf(stderr, "too big a command: %u\n", fullsize);
    return 0;
  }
  if (uade_inputbytes < uc->size) {
    if ((get_more(uc->size) == 0))
      return 0;
  }
  uade_copy_from_inputbuffer(&uc->data, uc->size);
  return 1;
}


int uade_get_string_command(char *s, enum uade_command_t com, size_t maxlen)
{
  const size_t COMLEN = 4096;
  uint8_t commandbuf[COMLEN];
  struct uade_control *uc = (struct uade_control *) commandbuf;

  if ((uade_get_command(uc, COMLEN) == 0))
    return 0;
  if (uc->command != com)
    return -1;
  if (uc->size == 0)
    return -1;
  if (uc->size != (strlen(uc->data) + 1))
    return -1;
  strlcpy(s, uc->data, maxlen);
  return 1;
}


void uade_set_input_source(const char *input_source)
{
  if ((uade_input_fd = open(input_source, O_RDONLY)) < 0) {
    fprintf(stderr, "can not open input file %s: %s\n", input_source, strerror(errno));
    exit(-1);
  }
}


void uade_set_output_destination(const char *output_destination)
{
  if ((uade_output_fd = open(output_destination, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
    fprintf(stderr, "can not open output file %s: %s\n", output_destination, strerror(errno));
    exit(-1);
  }
}