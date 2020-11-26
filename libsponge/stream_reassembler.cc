#include "stream_reassembler.hh"

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity):
    _output(capacity),
    _capacity(capacity),
    _bytes_unassembled(0),
    _substr_waiting({}),
    _flag_eof(false),
    _pos_eof(0) {}

void StreamReassembler::insert_substr_waiting(const struct Node &node){
    if (_substr_waiting.empty()) {
        _substr_waiting.insert(node);
        _bytes_unassembled += node.data.size();
        return;
    }

    struct Node tmp = node;
    auto it = _substr_waiting.lower_bound(node);   // lower_bound返回不小于目标值的第一个对象的迭代器
    size_t x = tmp.index, sz = tmp.data.size();
    // 若node的左边有节点，考察是否能与左边的节点合并
    if (it != _substr_waiting.begin()) {
        it--;   // 定位到左边那个节点
        if (x < it->index + it->data.size() ) {     // 若node与左边相交（相邻不算）或被包含
            if (x + sz <= it->index + it->data.size())      // 若被包含，直接丢弃，否则就是相交
                return;
            tmp.data = it->data + tmp.data.substr(it->index + it->data.size() - x);
            tmp.index = it->index;
            x = tmp.index; sz = tmp.data.size();
            _bytes_unassembled -= it->data.size();
            _substr_waiting.erase(it++);
        } else
            it++;
    }
    // 考察是否能与右边的节点合并，可能与多个节点合并
    while (it != _substr_waiting.end() && x + sz > it->index) {
        if (x >= it->index && x + sz < it->index + it->data.size()) // 若被右边包含，直接丢弃
            return;
        if (x + sz < it->index + it->data.size()) {     // 若与右边相交
            tmp.data += it->data.substr(x + sz - it->index);
        }
        _bytes_unassembled -= it->data.size();   // 相交或包含右边都需要移除节点
        _substr_waiting.erase(it++);
    }
    _substr_waiting.insert(tmp);      // tmp是检查合并后的新节点，也有可能没有发生任何合并操作
    _bytes_unassembled += tmp.data.size();
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    struct Node node{data, index};
    size_t first_unread = _output.bytes_read();
    size_t first_unassembled = _output.bytes_written();
    size_t first_unaccept = first_unread + _capacity;

    if (index + data.size() < first_unassembled || index >= first_unaccept)  // 超出这个范围不作处理
        return;
    if (index + data.size() > first_unaccept)
        node.data = node.data.substr(0, first_unaccept - index); // 若超出capacity，截下要处理的部分

    if (index <= first_unassembled) {   // 若新子串可以直接写入
        _output.write(node.data.substr(first_unassembled - index));
        // 检查缓冲区中的子串能否继续写入
        auto it = _substr_waiting.begin();
        while (it->index <= _output.bytes_written()) {
            if (it->index + it->data.size() > node.index + node.data.size()) // 被包含就不用写入了
                _output.write(it->data.substr(_output.bytes_written() - it->index));
            _bytes_unassembled -= it->data.size();
            _substr_waiting.erase(it++);
        }
    } else {
        insert_substr_waiting(node);    // 若不能写入则存入缓冲区
    }

    if (eof) {
        _flag_eof = true;
        _pos_eof = index + data.size(); 
    }
    if (_flag_eof && _output.bytes_written() == _pos_eof)
        _output.end_input();   
}

size_t StreamReassembler::unassembled_bytes() const { return _bytes_unassembled; }

bool StreamReassembler::empty() const { return _bytes_unassembled == 0; }
