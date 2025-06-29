package main

import (
	"fmt"
	"log"
	"os"
	"strconv"
	"time"

	"github.com/racerxd/mhdumper/fwpkt"
	"go.bug.st/serial"
)

func GetPacket(port serial.Port) *fwpkt.Packet {
	for {
		packet, err := fwpkt.ReadPacket(port)
		if err != nil {
			fmt.Printf("! Error reading packet: %v\n", err)
			panic(err)
		}
		if !packet.IsValid() {
			fmt.Printf("! Invalid packet received: %s\n", packet)
			panic("Invalid packet received")
		}

		if packet.Header == fwpkt.PacketTypeMessageResponse || packet.Header == fwpkt.PacketTypeMessage {
			fmt.Printf("> %s\n", packet.GetMessage())
			continue
		}
		return packet
	}
}

func DumpChunk(port serial.Port, addr uint32, length uint32) []byte {
	fwpkt.NewMemReadRequest(addr, length).ToPacket().WriteTo(port)
	packet := GetPacket(port)
	if packet.Header != fwpkt.PacketTypeReadMemoryResponse {
		log.Fatalf("Invalid memory read response packet received: %s", packet)
	}

	if len(packet.Data) < int(length) {
		log.Fatalf("Received memory data length %d is less than requested %d", len(packet.Data), length)
	}
	return packet.Data[:length]
}

func main() {
	args := os.Args[1:]
	if len(args) < 1 {
		fmt.Println("Usage: dumper <device> [--readmem <address> <length>]")
		return
	}

	device := args[0]
	var readMemAddr, readMemLength uint64
	if len(args) > 1 && args[1] == "--readmem" {
		if len(args) < 4 {
			fmt.Println("! Usage: dumper <device> --readmem <address> <length>")
			return
		}
		var err error
		readMemAddr, err = strconv.ParseUint(args[2], 0, 32)
		if err != nil {
			fmt.Printf("! Invalid address: %s\n", args[2])
			return
		}
		readMemLength, err = strconv.ParseUint(args[3], 0, 32)
		if err != nil {
			fmt.Printf("! Invalid length: %s\n", args[3])
			return
		}
	}

	if readMemLength == 0 {
		fmt.Println("! No memory read requested.")
		return
	}

	fmt.Printf("> Connecting to device %s...\n", device)
	mode := &serial.Mode{
		BaudRate: 115200,
	}
	port, err := serial.Open(device, mode)
	if err != nil {
		log.Fatal(err)
	}
	defer port.Close()

	port.SetReadTimeout(5 * time.Second) // Set read timeout
	fmt.Println("> Resetting device...")
	port.SetRTS(true)
	time.Sleep(100 * time.Millisecond) // Wait for device to reset
	port.SetRTS(false)
	port.ResetInputBuffer()
	port.ResetOutputBuffer()
	time.Sleep(100 * time.Millisecond)

	// Wait for first message
	packet, err := fwpkt.ReadPacket(port)
	if err != nil {
		log.Fatal(err)
	}

	if !packet.IsValid() || packet.Header != fwpkt.PacketTypeMessageResponse {
		log.Fatal("Invalid packet received")
	}

	fmt.Printf("> %s\n", packet.GetMessage())
	fwpkt.NewPacket(fwpkt.PacketTypeDevInfo, nil).WriteTo(port)
	packet = GetPacket(port)
	if packet.Header != fwpkt.PacketTypeDevInfoResponse {
		log.Fatal("Invalid device info packet received")
	}
	devInfo := fwpkt.GetPacketAs[fwpkt.DeviceInfo](packet)
	if devInfo == nil {
		log.Fatal("Failed to parse device info")
	}
	fmt.Printf("> Chip ID: 0x%08X\n", devInfo.ChipID)
	fmt.Printf("> Reading memory from 0x%08X, length: %d bytes...\n", readMemAddr, readMemLength)
	chunks := readMemLength / 4096
	if readMemLength%4096 != 0 {
		chunks++
	}
	fmt.Printf("> Total chunks: %d\n", chunks)
	data := make([]byte, 0, readMemLength)
	for i := uint64(0); i < chunks; i++ {
		addr := readMemAddr + i*4096
		length := uint32(4096)
		if i == chunks-1 {
			length = uint32(readMemLength % 4096)
			if length == 0 {
				length = 4096 // Last chunk can be full size if remainder is zero
			}
		}
		fmt.Printf("> Reading chunk %d/%d at 0x%08X, length: %d bytes...\n", i+1, chunks, addr, length)
		chunkData := DumpChunk(port, uint32(addr), length)
		data = append(data, chunkData...)
	}

	outputFile := "memory_dump.bin"
	err = os.WriteFile(outputFile, data, 0644)
	if err != nil {
		log.Fatalf("Failed to write memory dump to file: %v", err)
	}
	fmt.Printf("> Memory dump saved to %s\n", outputFile)
	fmt.Println("> Done.")
	os.Exit(0)
}
