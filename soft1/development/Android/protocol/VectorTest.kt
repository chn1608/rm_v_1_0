package rm.protocol

import java.io.File

private fun String.bytes() = chunked(2).map { it.toInt(16).toByte() }.toByteArray()

fun main(args: Array<String>) {
    check(crc16("123456789".toByteArray()) == 0x29b1)
    val rows = File(args.single()).readLines().filter { it.isNotBlank() && !it.startsWith("#") }
    check(rows.isNotEmpty())
    for (line in rows) {
        val (command, sequence, payload, wire) = line.split(" ")
        val want = Frame(command.toInt(16), sequence.toInt(16),
            if (payload == "-") byteArrayOf() else payload.bytes())
        val bytes = wire.bytes()
        check(encode(want).contentEquals(bytes))
        val decoder = Decoder()
        val frames = mutableListOf<Frame>()
        val corrupted = bytes.copyOf()
        corrupted[corrupted.lastIndex] = (corrupted.last().toInt() xor 0x10).toByte()
        decoder.feed(corrupted, 0) { frames.add(it) }
        check(frames.isEmpty() && decoder.badFrames == 1)
        decoder.feed(bytes.copyOfRange(0, 4), 1) { frames.add(it) }
        decoder.feed(bytes.copyOfRange(4, bytes.size) + bytes, 2) { frames.add(it) }
        check(frames.size == 2)
        for (frame in frames) {
            check(frame.kind == want.kind && frame.sequence == want.sequence)
            check(frame.payload.contentEquals(want.payload))
        }
    }
    println("Kotlin codec passed ${rows.size} shared vectors")
}
