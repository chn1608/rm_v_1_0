package rm.protocol

const val MAX_PAYLOAD = 64
private const val VERSION = 0x10
private const val HEADER_A = 0xA5
private const val HEADER_B = 0x5A

data class Frame(val kind: Int, val sequence: Int, val payload: ByteArray = byteArrayOf())

private fun Byte.unsigned() = toInt() and 0xff
private fun ByteArray.u16(offset: Int) = this[offset].unsigned() or
    (this[offset + 1].unsigned() shl 8)

fun crc16(data: ByteArray, offset: Int = 0, end: Int = data.size): Int {
    var crc = 0xffff
    for (i in offset until end) {
        crc = crc xor (data[i].unsigned() shl 8)
        repeat(8) {
            val poly = if (crc and 0x8000 != 0) 0x1021 else 0
            crc = ((crc shl 1) xor poly) and 0xffff
        }
    }
    return crc
}

fun encode(frame: Frame): ByteArray {
    require(frame.kind in 0..255 && frame.sequence in 0..65535)
    require(frame.payload.size <= MAX_PAYLOAD)
    val bytes = ByteArray(frame.payload.size + 9)
    bytes[0] = HEADER_A.toByte()
    bytes[1] = HEADER_B.toByte()
    bytes[2] = VERSION.toByte()
    bytes[3] = frame.kind.toByte()
    bytes[4] = frame.payload.size.toByte()
    bytes[5] = frame.sequence.toByte()
    bytes[6] = (frame.sequence shr 8).toByte()
    frame.payload.copyInto(bytes, destinationOffset = 7)
    val crc = crc16(bytes, offset = 2, end = bytes.size - 2)
    bytes[bytes.size - 2] = crc.toByte()
    bytes[bytes.size - 1] = (crc shr 8).toByte()
    return bytes
}

class Decoder {
    private val bytes = ByteArray(MAX_PAYLOAD + 9)
    private var used = 0
    private var lastMs = 0L
    var badFrames = 0
        private set

    fun reset() {
        used = 0
    }

    fun feed(data: ByteArray, nowMs: Long, onFrame: (Frame) -> Unit) {
        if (used != 0 && nowMs - lastMs >= 100) reset()
        if (data.isEmpty()) return
        lastMs = nowMs
        for (byte in data) {
            bytes[used++] = byte
            while (used > 0) {
                if (bytes[0].unsigned() != HEADER_A ||
                    (used >= 2 && bytes[1].unsigned() != HEADER_B)) {
                    drop(1)
                    continue
                }
                if (used < 5) break
                val size = bytes[4].unsigned() + 9
                if (bytes[2].unsigned() != VERSION || size > bytes.size ||
                    (used >= size && crc16(bytes, 2, size - 2) != bytes.u16(size - 2))) {
                    badFrames++
                    drop(1)
                    continue
                }
                if (used < size) break
                val frame = Frame(bytes[3].unsigned(), bytes.u16(5), bytes.copyOfRange(7, size - 2))
                drop(size)
                onFrame(frame)
            }
        }
    }

    private fun drop(count: Int) {
        bytes.copyInto(bytes, 0, count, used)
        used -= count
    }
}
