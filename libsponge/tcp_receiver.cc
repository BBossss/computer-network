#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    WrappingInt32 seqno(header.seqno);
    if (header.syn) {
        _isn = seqno;
        _isn_set = true;
        seqno = seqno + 1;
    }

    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t abs_seqno = unwrap(seqno, _isn, checkpoint);
    string data = seg.payload().copy();
    _reassembler.push_substring(data, abs_seqno - 1, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn_set) return {};
    if (_reassembler.stream_out().input_ended())
        return wrap(_reassembler.stream_out().bytes_written() + 1, _isn) + 1;
    else 
        return wrap(_reassembler.stream_out().bytes_written() + 1, _isn);
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().bytes_read() + _capacity - _reassembler.stream_out().bytes_written(); 
}
