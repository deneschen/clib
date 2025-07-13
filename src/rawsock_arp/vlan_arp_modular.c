#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h> // 使用固定宽度整数类型

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>

#define MY_ETH_ALEN 6
#define MY_ETHERTYPE_IP   0x0800
#define MY_ETHERTYPE_ARP  0x0806
#define MY_ETHERTYPE_VLAN 0x8100
#define MY_ARPHRD_ETHER   1
#define MY_ARPOP_REQUEST  1
#define MY_ARPOP_REPLY    2

#pragma pack(push, 1)
struct my_ethernet_header {
    uint8_t  dest_mac[MY_ETH_ALEN];
    uint8_t  src_mac[MY_ETH_ALEN];
    uint16_t ether_type;
};

struct my_vlan_header {
    uint16_t vlanid;
    uint16_t ethtype;
};

struct my_arp_packet {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t  hw_addr_len;
    uint8_t  proto_addr_len;
    uint16_t opcode;
    uint8_t  sender_mac[MY_ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[MY_ETH_ALEN];
    uint32_t target_ip;
};

struct my_full_vlan_arp_frame {
    struct my_ethernet_header eth_hdr;
    struct my_vlan_header     vlan_hdr;
    struct my_arp_packet      arp_pkt;
};
#pragma pack(pop)

struct network_interface {
    char name[IFNAMSIZ];
    int index;
    uint8_t mac[MY_ETH_ALEN];
    struct in_addr ip;
};

void print_mac(const char* label, const uint8_t* mac) {
    printf("%s: %02x:%02x:%02x:%02x:%02x:%02x\n", label,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int get_interface_info(int sockfd, struct network_interface* if_info) {
    struct ifreq ifr;
    strncpy(ifr.ifr_name, if_info->name, IFNAMSIZ - 1);

    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl(SIOCGIFINDEX)"); return -1;
    }
    if_info->index = ifr.ifr_ifindex;

    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl(SIOCGIFHWADDR)"); return -1;
    }
    memcpy(if_info->mac, ifr.ifr_hwaddr.sa_data, MY_ETH_ALEN);

    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1) {
        perror("ioctl(SIOCGIFADDR)"); return -1;
    }
    if_info->ip = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
    return 0;
}

int send_vlan_arp_request(int sockfd, const struct network_interface* if_info, int vlan_id, struct in_addr target_ip) {
    struct my_full_vlan_arp_frame frame;
    struct sockaddr_ll socket_address;

    const uint8_t broadcast_mac[MY_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t zero_mac[MY_ETH_ALEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memcpy(frame.eth_hdr.dest_mac, broadcast_mac, MY_ETH_ALEN);
    memcpy(frame.eth_hdr.src_mac, if_info->mac, MY_ETH_ALEN);
    frame.eth_hdr.ether_type = htons(MY_ETHERTYPE_VLAN);

    frame.vlan_hdr.ethtype = htons(MY_ETHERTYPE_ARP);
    frame.vlan_hdr.vlanid = htons(vlan_id & 0x0FFF);

    frame.arp_pkt.hardware_type = htons(MY_ARPHRD_ETHER);
    frame.arp_pkt.protocol_type = htons(MY_ETHERTYPE_IP);
    frame.arp_pkt.hw_addr_len = MY_ETH_ALEN;
    frame.arp_pkt.proto_addr_len = sizeof(struct in_addr);
    frame.arp_pkt.opcode = htons(MY_ARPOP_REQUEST);
    
    memcpy(frame.arp_pkt.sender_mac, if_info->mac, MY_ETH_ALEN);
    frame.arp_pkt.sender_ip = if_info->ip.s_addr; // s_addr已经是网络字节序
    memcpy(frame.arp_pkt.target_mac, zero_mac, MY_ETH_ALEN);
    frame.arp_pkt.target_ip = target_ip.s_addr;

    socket_address.sll_family = AF_PACKET;
    socket_address.sll_ifindex = if_info->index;
    socket_address.sll_halen = MY_ETH_ALEN;
    memcpy(socket_address.sll_addr, broadcast_mac, MY_ETH_ALEN);

    printf("发送 VLAN-tagged ARP 请求 (VLAN %d, 目标IP %s)...\n", vlan_id, inet_ntoa(target_ip));
    if (sendto(sockfd, &frame, sizeof(frame), 0, (struct sockaddr*)&socket_address, sizeof(socket_address)) < 0) {
        perror("sendto failed");
        return -1;
    }
    return 0;
}

int receive_vlan_arp_reply(int sockfd, const struct network_interface* if_info, int vlan_id, struct in_addr target_ip) {
    uint8_t buffer[1514]; // MTU
    
    printf("等待 ARP 响应...\n");
    while (1) {
        ssize_t num_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (num_bytes < 0) {
            perror("recvfrom failed");
            return -1;
        }

        if (num_bytes < sizeof(struct my_full_vlan_arp_frame)) continue;

        struct my_ethernet_header* eth_hdr = (struct my_ethernet_header*)buffer;

        if (memcmp(eth_hdr->dest_mac, if_info->mac, MY_ETH_ALEN) != 0 || 
            ntohs(eth_hdr->ether_type) != MY_ETHERTYPE_VLAN) {
            continue;
        }

        struct my_vlan_header* vlan_hdr = (struct my_vlan_header*)(buffer + sizeof(struct my_ethernet_header));
        uint16_t recv_vlan_id = ntohs(vlan_hdr->vlanid) & 0x0FFF;
        uint16_t* original_type_ptr = (uint16_t*)((uint8_t*)vlan_hdr + sizeof(struct my_vlan_header));
        
        if (recv_vlan_id != vlan_id || ntohs(*original_type_ptr) != MY_ETHERTYPE_ARP) {
            continue;
        }

        struct my_arp_packet* arp_pkt = (struct my_arp_packet*)((uint8_t*)original_type_ptr + sizeof(uint16_t));

        if (ntohs(arp_pkt->opcode) == MY_ARPOP_REPLY &&
            arp_pkt->target_ip == if_info->ip.s_addr &&
            arp_pkt->sender_ip == target_ip.s_addr) {
            
            struct in_addr final_sender_ip = { .s_addr = arp_pkt->sender_ip };
            printf("\n--- 成功收到匹配的 VLAN-tagged ARP 响应 ---\n");
            printf("来自 IP: %s\n", inet_ntoa(final_sender_ip));
            print_mac("其 MAC 地址是", arp_pkt->sender_mac);
            printf("VLAN ID: %d\n", recv_vlan_id);
            printf("------------------------------------------\n");
            return 0; // 成功
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "用法: %s <网络接口> <VLAN ID> <目标IP>\n", argv[0]);
        fprintf(stderr, "示例: sudo %s enp0s3 100 192.168.1.1\n", argv[0]);
        return 1;
    }

    const char* if_name_arg = argv[1];
    int vlan_id_arg = atoi(argv[2]);
    const char* target_ip_str_arg = argv[3];

    struct in_addr target_ip;
    if (inet_pton(AF_INET, target_ip_str_arg, &target_ip) != 1) {
        fprintf(stderr, "错误: 无效的目标IP地址\n"); return 1;
    }

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(MY_ETHERTYPE_VLAN));
    if (sockfd == -1) {
        perror("socket creation failed"); return 1;
    }

    struct network_interface local_if;
    strncpy(local_if.name, if_name_arg, IFNAMSIZ - 1);
    if (get_interface_info(sockfd, &local_if) == -1) {
        fprintf(stderr, "错误: 无法获取接口 '%s' 的信息。\n", if_name_arg);
        close(sockfd); return 1;
    }
    
    printf("--- 本地接口信息 ---\n");
    printf("接口名: %s\n", local_if.name);
    print_mac("MAC地址", local_if.mac);
    printf("IP地址: %s\n", inet_ntoa(local_if.ip));
    printf("---------------------\n\n");

    if (send_vlan_arp_request(sockfd, &local_if, vlan_id_arg, target_ip) == 0) {
        receive_vlan_arp_reply(sockfd, &local_if, vlan_id_arg, target_ip);
    } else {
        fprintf(stderr, "发送ARP请求失败。\n");
    }

    close(sockfd);
    return 0;
}
