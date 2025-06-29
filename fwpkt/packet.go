package fwpkt

import (
	"encoding/binary"
	"fmt"
	"io"
	"unsafe"
)

//go:generate stringer -type=PacketType -output=packet_string.go
type PacketType uint8

const (
	PacketTypeUnknown            PacketType = 0xFF
	PacketTypeDevInfo            PacketType = 0x01
	PacketTypeReadMemory         PacketType = 0x02
	PacketTypeMessage            PacketType = 0x03
	PacketTypeDevInfoResponse    PacketType = 0x81
	PacketTypeReadMemoryResponse PacketType = 0x82
	PacketTypeMessageResponse    PacketType = 0x83
)

type Packet struct {
	Magic  [4]byte
	Header PacketType
	Length uint16
	Data   []byte
}

func NewPacket(header PacketType, data []byte) *Packet {
	p := &Packet{
		Magic:  [4]byte{0xDE, 0xAD, 0xBE, 0xEF},
		Header: header,
		Length: uint16(len(data)),
		Data:   make([]byte, len(data)),
	}
	copy(p.Data, data)
	return p
}

func ReadPacket(reader io.Reader) (*Packet, error) {
	var magic [4]byte
	if _, err := io.ReadFull(reader, magic[:]); err != nil {
		return nil, err
	}

	if magic[0] != 0xDE || magic[1] != 0xAD || magic[2] != 0xBE || magic[3] != 0xEF {
		return nil, fmt.Errorf("invalid magic bytes: %x", magic)
	}
	tmp := make([]byte, 3)

	if _, err := io.ReadFull(reader, tmp); err != nil {
		return nil, err
	}
	header := PacketType(tmp[0])
	length := binary.LittleEndian.Uint16(tmp[1:3])

	data := make([]byte, length)
	if _, err := io.ReadFull(reader, data); err != nil {
		return nil, err
	}

	return &Packet{
		Magic:  magic,
		Header: header,
		Length: length,
		Data:   data,
	}, nil
}

func (p *Packet) IsValid() bool {
	if len(p.Magic) != 4 || p.Magic[0] != 0xDE || p.Magic[1] != 0xAD || p.Magic[2] != 0xBE || p.Magic[3] != 0xEF {
		return false
	}
	if p.Length != uint16(len(p.Data)) {
		return false
	}
	return true
}

func (p *Packet) ToBytes() []byte {
	data := make([]byte, 7+len(p.Data))
	copy(data[:4], p.Magic[:])
	data[4] = byte(p.Header)
	data[5] = byte(p.Length >> 8)
	data[6] = byte(p.Length)
	if len(p.Data) > 0 {
		copy(data[7:], p.Data)
	}
	return data
}

func (p *Packet) WriteTo(writer io.Writer) (int64, error) {
	data := p.ToBytes()
	n, err := writer.Write(data)
	if err != nil {
		return int64(n), err
	}
	if n != len(data) {
		return int64(n), fmt.Errorf("short write: expected %d bytes, wrote %d", len(data), n)
	}
	return int64(n), nil
}

func GetPacketAs[T any](p *Packet) *T {
	var result T
	tSize := unsafe.Sizeof(result)
	if tSize > 4096 || tSize < 1 {
		return nil // Return zero value of T if size is invalid
	}

	if len(p.Data) < int(tSize) {
		return nil // Not enough data to fill the struct
	}

	dataSlice := p.Data[:tSize]
	ptr := unsafe.Pointer(&result)
	copy((*[4096]byte)(ptr)[:tSize], dataSlice)
	return &result
}

func (p *Packet) String() string {
	if p == nil {
		return "<nil>"
	}
	data := "..."
	switch p.Header {
	case PacketTypeDevInfo:
		if info := GetPacketAs[DeviceInfo](p); info != nil {
			data = fmt.Sprintf("ChipID: 0x%08X", info.ChipID)
		}
	case PacketTypeMessage, PacketTypeMessageResponse:
		data = p.GetMessage()
	}
	return fmt.Sprintf("Packet{Header: %s, Length: %d, Data: %s}",
		p.Header, p.Length, data)
}
