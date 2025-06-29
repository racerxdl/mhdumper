#include <stdio.h>

extern "C" {
    #include "air105.h"
    #include "air105_otp.h"
    #include "air105_spi.h"
    #include "delay.h"
    #include "sysc.h"
}

#include "uart.h"
#include "fault.h"

#define BUFF_SIZE 5000

enum PacketType : uint8_t {
    PACKET_DEV_INFO = 0x01,
    PACKET_READ_MEMORY = 0x02,
    PACKET_MESSAGE = 0x03,

    // Responses
    PACKET_DEV_INFO_RESPONSE = 0x81,
    PACKET_READ_MEMORY_RESPONSE = 0x82,
    PACKET_MESSAGE_RESPONSE = 0x83,
};

struct DeviceInfo {
    uint32_t chip_id;
};

struct MemReadRequest {
    uint32_t address;
    uint32_t length;
};

struct Packet {
    uint8_t magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    PacketType header;
    uint16_t length;
    uint8_t data[BUFF_SIZE];
} __attribute__((packed));

// Packet state
uint16_t currentPos = 0;
uint8_t tmpBuffer[4];
uint32_t lastPacketByteTick = 0;

const uint32_t PacketTimeout = 100; // 100ms


void SendPacket(const Packet &packet) {
    uint16_t packetSize = sizeof(packet.magic) + sizeof(packet.header) + sizeof(packet.length) + packet.length;
    SerialSend((uint8_t*)&packet, packetSize);
}

void SendMessageBack(const char *message) {
    Packet packet;
    packet.header = PACKET_MESSAGE_RESPONSE;
    packet.length = strlen(message)+1; // +1 for null terminator
    if (packet.length >= BUFF_SIZE) {
        packet.length = BUFF_SIZE-1;
    }
    memcpy(packet.data, message, packet.length);

    SendPacket(packet);
}

void TryReceivePacket() {
    // Reset state
    lastPacketByteTick = get_tick() / 1000;
    currentPos = 0;

    // Wait for the magic bytes
    while (currentPos < 4) {
        if (!SerialIsRXEmpty()) {
            tmpBuffer[currentPos] = SerialGetByte();
            currentPos++;
            lastPacketByteTick = get_tick() / 1000;
            continue;
        }
        if (get_tick() / 1000 - lastPacketByteTick > PacketTimeout) {
            // Timeout, bail out
            return;
        }
    }

    // Check magic bytes
    if (tmpBuffer[0] != 0xDE || tmpBuffer[1] != 0xAD ||
        tmpBuffer[2] != 0xBE || tmpBuffer[3] != 0xEF) {
        // Invalid magic bytes, reset state
        currentPos = 0;
        return;
    }

    // Read the header + length
    currentPos = 0;
    while (currentPos < 3) { // PackeType + length
        if (!SerialIsRXEmpty()) {
            tmpBuffer[currentPos] = SerialGetByte();
            currentPos++;
            lastPacketByteTick = get_tick() / 1000;
            continue;
        }
        if (get_tick() / 1000 - lastPacketByteTick > PacketTimeout) {
            // Timeout, bail out
            return;
        }
    }

    Packet packet;
    packet.header = static_cast<PacketType>(tmpBuffer[0]);
    packet.length = (tmpBuffer[1] << 8) | tmpBuffer[2];
    if (packet.length > sizeof(packet.data)) {
        SendMessageBack("Invalid packet length");
        // Invalid length, reset state
        currentPos = 0;
        return;
    }

    // Read the data
    currentPos = 0;
    while (currentPos < packet.length) {
        if (!SerialIsRXEmpty()) {
            packet.data[currentPos] = SerialGetByte();
            currentPos++;
            lastPacketByteTick = get_tick() / 1000;
            continue;
        }
    }

    // We have everything
    SendMessageBack("Packet received successfully");
    // Process the packet based on its type
    switch (packet.header) {
        case PACKET_DEV_INFO: {
            DeviceInfo dev_info;
            dev_info.chip_id = SYSCTRL->CHIP_ID;
            Packet response_packet;
            response_packet.header = PACKET_DEV_INFO_RESPONSE;
            response_packet.length = sizeof(dev_info);
            memcpy(response_packet.data, &dev_info, sizeof(dev_info));
            SendPacket(response_packet);
            break;
        }

        case PACKET_READ_MEMORY: {
            MemReadRequest mem_request;
            if (packet.length < sizeof(MemReadRequest)) {
                SendMessageBack("Invalid memory read request");
                return;
            }
            memcpy(&mem_request, packet.data, sizeof(MemReadRequest));

            if (mem_request.length >= BUFF_SIZE) {
                sprintf((char *)packet.data, "Memory read request length exceeds buffer size: %d\n", mem_request.length);
                SendMessageBack((const char *)packet.data);
                return;
            }

            Packet response_packet;
            response_packet.header = PACKET_READ_MEMORY_RESPONSE;
            response_packet.length = mem_request.length;
            // Make length a multiple of 4 for safe reading
            if (mem_request.length % 4 != 0) {
                mem_request.length += (4 - (mem_request.length % 4));
            }
            response_packet.length = (mem_request.length > sizeof(response_packet.data)) ? sizeof(response_packet.data) : mem_request.length;

            SafeMemoryRead(mem_request.address, response_packet.data, response_packet.length);
            SendPacket(response_packet);
            break;
        }
        default:
            // Unknown packet type, send back an error message
            SendMessageBack("Unknown packet type");
            break;
    }
}

int main(void) {
    SystemClock_Config_HSE();
    Delay_Init();
    SYSCTRL->CG_CTRL1 = SYSCTRL_APBPeriph_ALL;
    SYSCTRL->CG_CTRL2 = SYSCTRL_AHBPeriph_ALL;

    GPIO_PinRemapConfig(GPIOA, GPIO_Pin_0 | GPIO_Pin_1,GPIO_Remap_0);
	SYSCTRL_APBPeriphClockCmd(SYSCTRL_APBPeriph_UART0, ENABLE);
	SYSCTRL_APBPeriphResetCmd(SYSCTRL_APBPeriph_UART0, ENABLE);

    UART_InitTypeDef uart_init;
    uart_init.UART_BaudRate = 115200;
    uart_init.UART_WordLength = UART_WordLength_8b;
    uart_init.UART_StopBits = UART_StopBits_1;
    uart_init.UART_Parity = UART_Parity_No;
    UART_Init(UART0, &uart_init);
    UART_ITConfig(UART0, UART_IT_RX_RECVD, ENABLE);
    NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = UART0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

    Delay_ms(1000); // Wait for a bit
    SendMessageBack("MHROMDUMPER Firmware v1.0");

    // Main loop
    while (1) {
        TryReceivePacket();
    }
}