#define _5_cpp

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "liburing.h"
#include "0.h"

#define MAX_CLIENT_NUMBER 32
#define MAX_CONNECTIONS 64
#define BACKLOG 512
#define MAX_MESSAGE_LEN 1025
#define BUFFERS_COUNT MAX_CONNECTIONS

void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, unsigned flags);
void add_socket_read(struct io_uring *ring, int fd, unsigned gid, size_t size, unsigned flags);
void add_socket_write(struct io_uring *ring, int fd, __u16 bid, size_t size, unsigned flags);
void add_provide_buf(struct io_uring *ring, __u16 bid, unsigned gid);

enum
{
    ACCEPT,
    READ,
    WRITE,
    PROV_BUF,
};

typedef struct conn_info
{
    __u32 fd;
    __u16 type;
    __u16 bid;
} conn_info;

char bufs[BUFFERS_COUNT][MAX_MESSAGE_LEN] = {0};
int group_id = 1337;

typedef struct Client_uring
{
    int connected;
    int fd;
} Client_uring;

Client_uring clients_list[MAX_CLIENT_NUMBER];
int connected_clients_num = 0;

void display_client()
{
    // for (int i = 0; i < MAX_CLIENT_NUMBER; i++)
    // {
    //     printf(" sock index:%d, fd:%d, connected:%d\n", i, clients_list[i].fd, clients_list[i].connected);
    // }
}

void client_destroy(int fd) {
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
        if (clients_list[i].connected && clients_list[i].fd == fd) {
            clients_list[i].connected = 0;
            connected_clients_num--;
        }
    }
}


int client_add(int fd)
{
    int index = -1;
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++)
    {
        if (!clients_list[i].connected)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
    {
        return -1;
    }
    // fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    clients_list[index].connected = 1;
    clients_list[index].fd = fd;
    connected_clients_num++;

    printf("fd:%d, index:%d\n", fd, index);

    return index;
}

int main(int argc, char *argv[])
{
    int port = atoi(argv[1]);
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // setup socket
    int sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    const int val = 1;
    setsockopt(sock_listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind and listen
    if (bind(sock_listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(sock_listen_fd, BACKLOG) < 0)
    {
        perror("listen");
        return 1;
    }
    printf("io_uring server listening for connections on port: %d\n", port);

    // initialize io_uring
    struct io_uring_params params;
    struct io_uring ring;
    memset(&params, 0, sizeof(params));

    if (io_uring_queue_init_params(2048, &ring, &params) < 0)
    {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    // check if IORING_FEAT_FAST_POLL is supported
    if (!(params.features & IORING_FEAT_FAST_POLL))
    {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    // check if buffer selection is supported
    struct io_uring_probe *probe;
    probe = io_uring_get_probe_ring(&ring);
    if (!probe || !io_uring_opcode_supported(probe, IORING_OP_PROVIDE_BUFFERS))
    {
        printf("Buffer select not supported, skipping...\n");
        exit(0);
    }
    free(probe);

    // register buffers for buffer selection
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_provide_buffers(sqe, bufs, MAX_MESSAGE_LEN, BUFFERS_COUNT, group_id, 0);

    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0)
    {
        printf("cqe->res = %d\n", cqe->res);
        exit(1);
    }
    io_uring_cqe_seen(&ring, cqe);

    // add first accept SQE to monitor for new incoming connections
    add_accept(&ring, sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);

    // 初始化用户列表
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++)
    {
        clients_list[i].connected = 0;
    }

    // start event loop
    while (1)
    {
        io_uring_submit_and_wait(&ring, 1);
        struct io_uring_cqe *cqe;
        unsigned head;
        unsigned count = 0;

        // go through all CQEs
        io_uring_for_each_cqe(&ring, head, cqe)
        {
            ++count;
            struct conn_info conn_i;
            memcpy(&conn_i, &cqe->user_data, sizeof(conn_i));

            int type = conn_i.type;
            
            // 相当于获得了events.data
            if (type == PROV_BUF)
            {
                if (cqe->res < 0)
                {
                    printf("error: cqe->res = %d\n", cqe->res);
                    exit(1);
                }
            }
            else if (type == ACCEPT)
            {
                int sock_conn_fd = cqe->res;
                // only read when there is no error, >= 0
                if (sock_conn_fd >= 0)
                {
                    add_socket_read(&ring, sock_conn_fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
                }

                // new connected client; read data from socket and re-add accept to monitor for new connections
                add_accept(&ring, sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);
                client_add(sock_conn_fd);
            }
            else if (type == READ)
            {
                int bytes_read = cqe->res;
                int bid = cqe->flags >> 16;
                if (cqe->res <= 0)
                {
                    // read failed, re-add the buffer
                    add_provide_buf(&ring, bid, group_id);
                    // connection closed or error
                    close(conn_i.fd);
                    client_destroy(conn_i.fd);
                }
                else
                {
                    add_socket_read(&ring, conn_i.fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
                    int tmp_fd = conn_i.fd;
                    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
                        if (clients_list[i].connected && clients_list[i].fd != tmp_fd) {
                            add_socket_write(&ring, clients_list[i].fd, bid, bytes_read, 0);
                        }
                    }
                    conn_info conn_i = {
                        .fd = tmp_fd,
                        .type = READ,
                    };
                    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
                }
            }
            else if (type == WRITE)
            {
                // write has been completed, first re-add the buffer
                add_provide_buf(&ring, conn_i.bid, group_id);
                // add a new read for the existing connection
                //add_socket_read(&ring, conn_i.fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
            }
        }
        io_uring_cq_advance(&ring, count);
    }
}

void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, unsigned flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = ACCEPT,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_socket_read(struct io_uring *ring, int fd, unsigned gid, size_t message_size, unsigned flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, NULL, message_size, 0);
    // printf("fd%d try to recv message from %d\n", fd, fd);
    io_uring_sqe_set_flags(sqe, flags);
    sqe->buf_group = gid;

    conn_info conn_i = {
        .fd = fd,
        .type = READ,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_socket_write(struct io_uring *ring, int fd, __u16 bid, size_t message_size, unsigned flags)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_send(sqe, fd, &bufs[bid], message_size, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = WRITE,
        .bid = bid,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_provide_buf(struct io_uring *ring, __u16 bid, unsigned gid)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_provide_buffers(sqe, bufs[bid], MAX_MESSAGE_LEN, 1, gid, bid);

    conn_info conn_i = {
        .fd = 0,
        .type = PROV_BUF,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}
