package fwpkt

func (p *Packet) GetMessage() string {
	if p.Header != PacketTypeMessage && p.Header != PacketTypeMessageResponse {
		return ""
	}
	if len(p.Data) < 1 {
		return ""
	}
	return string(p.Data[:len(p.Data)-1]) // Exclude the last byte for null terminator
}
