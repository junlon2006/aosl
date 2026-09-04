#include <string.h>

#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_iomp.h>
#include <hal/aosl_hal_memory.h>

#include <lwip/sockets.h>

int aosl_hal_epoll_create(void)
{
  return -1;
}

int aosl_hal_epoll_destroy(int epfd)
{
  (void)epfd;
  return -1;
}

int aosl_hal_epoll_ctl(int epfd, aosl_epoll_op_e op, aosl_fd_t fd,
                       aosl_poll_event_t *ev)
{
  (void)epfd;
  (void)op;
  (void)fd;
  (void)ev;
  return -1;
}

int aosl_hal_epoll_wait(int epfd, aosl_poll_event_t *evlist, int maxevents,
                        int timeout_ms)
{
  (void)epfd;
  (void)evlist;
  (void)maxevents;
  (void)timeout_ms;
  return -1;
}

int aosl_hal_poll(aosl_poll_event_t fds[], int nfds, int timeout_ms)
{
  (void)fds;
  (void)nfds;
  (void)timeout_ms;
  return -1;
}

fd_set_t aosl_hal_fdset_create(void)
{
  fd_set *fds = (fd_set *)aosl_hal_malloc(sizeof(*fds));

  if (fds != NULL) {
    FD_ZERO(fds);
  }
  return (fd_set_t)fds;
}

void aosl_hal_fdset_destroy(fd_set_t fdset)
{
  aosl_hal_free(fdset);
}

void aosl_hal_fdset_zero(fd_set_t fdset)
{
  if (fdset != NULL) {
    FD_ZERO((fd_set *)fdset);
  }
}

void aosl_hal_fdset_set(fd_set_t fdset, aosl_fd_t fd)
{
  if (fdset != NULL && !aosl_fd_invalid(fd)) {
    FD_SET(fd, (fd_set *)fdset);
  }
}

void aosl_hal_fdset_clr(fd_set_t fdset, aosl_fd_t fd)
{
  if (fdset != NULL && !aosl_fd_invalid(fd)) {
    FD_CLR(fd, (fd_set *)fdset);
  }
}

int aosl_hal_fdset_isset(fd_set_t fdset, aosl_fd_t fd)
{
  if (fdset == NULL || aosl_fd_invalid(fd)) {
    return 0;
  }
  return FD_ISSET(fd, (fd_set *)fdset) ? 1 : 0;
}

int aosl_hal_select(int nfds, fd_set_t readfds, fd_set_t writefds,
                    fd_set_t exceptfds, int timeout_ms)
{
  struct timeval timeout;
  struct timeval *timeout_ptr = NULL;
  int result;

  if (nfds < 0) {
    return AOSL_HAL_RET_EHAL;
  }
  if (timeout_ms >= 0) {
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    timeout_ptr = &timeout;
  }

  result = lwip_select(nfds, (fd_set *)readfds, (fd_set *)writefds,
                       (fd_set *)exceptfds, timeout_ptr);
  if (result < 0) {
    return aosl_hal_errno_convert(lwip_getthreaderrno(-1));
  }
  return result;
}
