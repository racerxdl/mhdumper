package fwpkt

/*
	struct DeviceInfo {
	    uint32_t chip_id;
	};
*/
type DeviceInfo struct {
	ChipID uint32
}

func NewDeviceInfo(chipID uint32) *DeviceInfo {
	return &DeviceInfo{
		ChipID: chipID,
	}
}

func (d *DeviceInfo) ToPacket() *Packet {
	data := make([]byte, 4)
	data[0] = byte(d.ChipID >> 24)
	data[1] = byte(d.ChipID >> 16)
	data[2] = byte(d.ChipID >> 8)
	data[3] = byte(d.ChipID)

	return NewPacket(PacketTypeDevInfo, data)
}
