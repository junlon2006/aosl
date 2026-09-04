#include <string.h>

#include <hal/aosl_hal_errno.h>
#include <hal/aosl_hal_socket.h>

#include <lwip/netdb.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>

static int socket_error(aosl_fd_t fd)
{
  return aosl_hal_errno_convert(lwip_getthreaderrno((int)fd));
}

static int socket_domain(enum aosl_socket_domain domain)
{
  switch (domain) {
    case AOSL_AF_UNSPEC:
      return AF_UNSPEC;
    case AOSL_AF_INET:
      return AF_INET;
#if LWIP_IPV6
    case AOSL_AF_INET6:
      return AF_INET6;
#endif
    default:
      return -1;
  }
}

static int socket_type(enum aosl_socket_type type)
{
  switch (type) {
    case AOSL_SOCK_STREAM:
      return SOCK_STREAM;
    case AOSL_SOCK_DGRAM:
      return SOCK_DGRAM;
    default:
      return -1;
  }
}

static int socket_protocol(enum aosl_socket_proto protocol)
{
  switch (protocol) {
    case AOSL_IPPROTO_AUTO:
      return 0;
    case AOSL_IPPROTO_TCP:
      return IPPROTO_TCP;
    case AOSL_IPPROTO_UDP:
      return IPPROTO_UDP;
    default:
      return -1;
  }
}

static socklen_t sockaddr_from_aosl(const aosl_sockaddr_t *source,
                                    struct sockaddr_storage *storage)
{
  if (source == NULL || storage == NULL) {
    return 0;
  }

  memset(storage, 0, sizeof(*storage));
  switch (source->sa_family) {
    case AOSL_AF_INET: {
      struct sockaddr_in *address = (struct sockaddr_in *)storage;
      address->sin_len = (u8_t)sizeof(*address);
      address->sin_family = AF_INET;
      address->sin_port = source->sa_port;
      address->sin_addr.s_addr = source->sin_addr;
      return (socklen_t)sizeof(*address);
    }
#if LWIP_IPV6
    case AOSL_AF_INET6: {
      struct sockaddr_in6 *address = (struct sockaddr_in6 *)storage;
      address->sin6_len = (u8_t)sizeof(*address);
      address->sin6_family = AF_INET6;
      address->sin6_port = source->sa_port;
      address->sin6_flowinfo = source->sin6_flowinfo;
      memcpy(&address->sin6_addr, source->sin6_addr,
             sizeof(source->sin6_addr));
      return (socklen_t)sizeof(*address);
    }
#endif
    default:
      return 0;
  }
}

static int sockaddr_to_aosl(const struct sockaddr *source,
                            aosl_sockaddr_t *destination)
{
  if (source == NULL || destination == NULL) {
    return -1;
  }

  memset(destination, 0, sizeof(*destination));
  switch (source->sa_family) {
    case AF_INET: {
      const struct sockaddr_in *address = (const struct sockaddr_in *)source;
      destination->sa_family = AOSL_AF_INET;
      destination->sa_port = address->sin_port;
      destination->sin_addr = address->sin_addr.s_addr;
      return 0;
    }
#if LWIP_IPV6
    case AF_INET6: {
      const struct sockaddr_in6 *address = (const struct sockaddr_in6 *)source;
      destination->sa_family = AOSL_AF_INET6;
      destination->sa_port = address->sin6_port;
      destination->sin6_flowinfo = address->sin6_flowinfo;
      memcpy(destination->sin6_addr, &address->sin6_addr,
             sizeof(destination->sin6_addr));
      /* This lwIP sockaddr_in6 has no scope-id member. */
      destination->sin6_scope_id = 0;
      return 0;
    }
#endif
    default:
      return -1;
  }
}

aosl_fd_t aosl_hal_sk_socket(enum aosl_socket_domain domain,
                             enum aosl_socket_type type,
                             enum aosl_socket_proto protocol)
{
  int os_domain = socket_domain(domain);
  int os_type = socket_type(type);
  int os_protocol = socket_protocol(protocol);
  int fd;

  if (os_domain < 0 || os_type < 0 || os_protocol < 0) {
    return AOSL_INVALID_FD;
  }

  fd = lwip_socket(os_domain, os_type, os_protocol);
  return fd < 0 ? AOSL_INVALID_FD : (aosl_fd_t)fd;
}

int aosl_hal_sk_bind(aosl_fd_t sockfd, const aosl_sockaddr_t *addr)
{
  struct sockaddr_storage storage;
  socklen_t length = sockaddr_from_aosl(addr, &storage);

  if (aosl_fd_invalid(sockfd) || length == 0) {
    return AOSL_HAL_RET_EHAL;
  }
  if (lwip_bind((int)sockfd, (const struct sockaddr *)&storage, length) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

int aosl_hal_sk_bind_device(aosl_fd_t sockfd, const char *if_name)
{
  size_t length;

  if (aosl_fd_invalid(sockfd) || if_name == NULL || if_name[0] == '\0') {
    return AOSL_HAL_RET_EHAL;
  }

  length = strlen(if_name);
  if (lwip_setsockopt((int)sockfd, SOL_SOCKET, SO_BINDTODEVICE,
                      if_name, (socklen_t)length) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

int aosl_hal_sk_set_dscp(aosl_fd_t sockfd,
                         enum aosl_socket_domain domain, uint8_t dscp)
{
  int tos;

  if (aosl_fd_invalid(sockfd) || dscp > 63 || domain != AOSL_AF_INET) {
    return AOSL_HAL_RET_EHAL;
  }

  tos = ((int)dscp) << 2;
  if (lwip_setsockopt((int)sockfd, IPPROTO_IP, IP_TOS,
                      &tos, (socklen_t)sizeof(tos)) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

int aosl_hal_sk_listen(aosl_fd_t sockfd, int backlog)
{
  if (aosl_fd_invalid(sockfd) || lwip_listen((int)sockfd, backlog) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

aosl_fd_t aosl_hal_sk_accept(aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
  struct sockaddr_storage storage;
  struct sockaddr *address = addr != NULL ? (struct sockaddr *)&storage : NULL;
  socklen_t length = (socklen_t)sizeof(storage);
  socklen_t *length_ptr = addr != NULL ? &length : NULL;
  int accepted;

  if (aosl_fd_invalid(sockfd)) {
    return AOSL_INVALID_FD;
  }

  memset(&storage, 0, sizeof(storage));
  accepted = lwip_accept((int)sockfd, address, length_ptr);
  if (accepted < 0) {
    (void)socket_error(sockfd);
    return AOSL_INVALID_FD;
  }
  if (addr != NULL && sockaddr_to_aosl(address, addr) != 0) {
    (void)lwip_close(accepted);
    return AOSL_INVALID_FD;
  }
  return (aosl_fd_t)accepted;
}

int aosl_hal_sk_connect(aosl_fd_t sockfd, const aosl_sockaddr_t *addr)
{
  struct sockaddr_storage storage;
  socklen_t length = sockaddr_from_aosl(addr, &storage);

  if (aosl_fd_invalid(sockfd) || length == 0) {
    return AOSL_HAL_RET_EHAL;
  }
  if (lwip_connect((int)sockfd, (const struct sockaddr *)&storage, length) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

int aosl_hal_sk_close(aosl_fd_t sockfd)
{
  if (aosl_fd_invalid(sockfd)) {
    return -1;
  }
  if (lwip_close((int)sockfd) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

int aosl_hal_sk_send(aosl_fd_t sockfd, const void *buf, size_t len, int flags)
{
  int result = lwip_send((int)sockfd, buf, len, flags);
  return result < 0 ? socket_error(sockfd) : result;
}

int aosl_hal_sk_recv(aosl_fd_t sockfd, void *buf, size_t len, int flags)
{
  int result = lwip_recv((int)sockfd, buf, len, flags);
  return result < 0 ? socket_error(sockfd) : result;
}

int aosl_hal_sk_sendto(aosl_fd_t sockfd, const void *buffer, size_t length,
                       int flags, const aosl_sockaddr_t *dest_addr)
{
  struct sockaddr_storage storage;
  socklen_t address_length = sockaddr_from_aosl(dest_addr, &storage);
  int result;

  if (aosl_fd_invalid(sockfd) || address_length == 0) {
    return AOSL_HAL_RET_EHAL;
  }
  result = lwip_sendto((int)sockfd, buffer, length, flags,
                       (const struct sockaddr *)&storage, address_length);
  return result < 0 ? socket_error(sockfd) : result;
}

int aosl_hal_sk_recvfrom(aosl_fd_t sockfd, void *buffer, size_t length,
                         int flags, aosl_sockaddr_t *src_addr)
{
  struct sockaddr_storage storage;
  struct sockaddr *address = src_addr != NULL ? (struct sockaddr *)&storage : NULL;
  socklen_t address_length = (socklen_t)sizeof(storage);
  socklen_t *length_ptr = src_addr != NULL ? &address_length : NULL;
  int result;

  memset(&storage, 0, sizeof(storage));
  result = lwip_recvfrom((int)sockfd, buffer, length, flags,
                         address, length_ptr);
  if (result < 0) {
    return socket_error(sockfd);
  }
  if (src_addr != NULL && sockaddr_to_aosl(address, src_addr) != 0) {
    return AOSL_HAL_RET_EHAL;
  }
  return result;
}

int aosl_hal_sk_read(aosl_fd_t sockfd, void *buf, size_t count)
{
  int result = lwip_read((int)sockfd, buf, count);
  return result < 0 ? socket_error(sockfd) : result;
}

int aosl_hal_sk_write(aosl_fd_t sockfd, const void *buf, size_t count)
{
  int result = lwip_write((int)sockfd, buf, count);
  return result < 0 ? socket_error(sockfd) : result;
}

int aosl_hal_sk_set_nonblock(aosl_fd_t sockfd)
{
  unsigned long enabled = 1;

  if (aosl_fd_invalid(sockfd)) {
    return -1;
  }
  if (lwip_ioctl((int)sockfd, FIONBIO, &enabled) < 0) {
    return socket_error(sockfd);
  }
  return 0;
}

int aosl_hal_sk_get_local_ip(aosl_sockaddr_t *addr)
{
  struct netif *netif;

  if (addr == NULL) {
    return -1;
  }

  netif = netif_default;
  if (netif == NULL || !netif_is_up(netif) ||
      !netif_is_ip4_cfg(netif) || netif_is_IF_LOOP(netif)) {
    return -1;
  }

  memset(addr, 0, sizeof(*addr));
  addr->sa_family = AOSL_AF_INET;
  addr->sin_addr = netif->ip_addr.addr;
  return 0;
}

int aosl_hal_sk_get_sockname(aosl_fd_t sockfd, aosl_sockaddr_t *addr)
{
  struct sockaddr_storage storage;
  socklen_t length = (socklen_t)sizeof(storage);

  if (aosl_fd_invalid(sockfd) || addr == NULL) {
    return -1;
  }
  memset(&storage, 0, sizeof(storage));
  if (lwip_getsockname((int)sockfd, (struct sockaddr *)&storage, &length) < 0) {
    return socket_error(sockfd);
  }
  return sockaddr_to_aosl((const struct sockaddr *)&storage, addr);
}

int aosl_hal_gethostbyname(const char *hostname, aosl_sockaddr_t *addrs,
                           int addr_count)
{
  struct addrinfo hints;
  struct addrinfo *results = NULL;
  struct addrinfo *item;
  int count = 0;

  if (hostname == NULL || hostname[0] == '\0' ||
      addrs == NULL || addr_count <= 0) {
    return 0;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if (lwip_getaddrinfo(hostname, NULL, &hints, &results) != 0) {
    return 0;
  }

  for (item = results; item != NULL && count < addr_count; item = item->ai_next) {
    if (item->ai_addr == NULL) {
      continue;
    }
    if (sockaddr_to_aosl(item->ai_addr, &addrs[count]) == 0) {
      ++count;
    }
  }
  lwip_freeaddrinfo(results);
  return count;
}
