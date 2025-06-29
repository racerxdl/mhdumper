package fwpkt

import "encoding/binary"

/*
struct MemReadRequest {
    uint32_t address;
    uint32_t length;
};
*/

type MemReadRequest struct {
	Address uint32
	Length  uint32
}

func NewMemReadRequest(address, length uint32) *MemReadRequest {
	return &MemReadRequest{
		Address: address,
		Length:  length,
	}
}

func (m *MemReadRequest) ToPacket() *Packet {
	data := make([]byte, 8)
	binary.LittleEndian.PutUint32(data[0:4], m.Address)
	binary.LittleEndian.PutUint32(data[4:8], m.Length)

	return NewPacket(PacketTypeReadMemory, data)
}
