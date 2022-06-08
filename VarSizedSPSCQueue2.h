/**
 * @ Author: wzyhft
 * @ Create Time: 2022-05-24 23:11:37
 * @ Description:
 */

#pragma once

#include <cstdio>
#include <cstdint>
#include <atomic>

namespace dao{
namespace lockfree_container{

class VarSizedSPSCQueue{

public:
    struct MsgHeader {
        uint16_t size;
        uint16_t unused;
        uint32_t timestamp;
    };
    static constexpr size_t MSG_HEADER_SIZE = sizeof(MsgHeader);

    MsgHeader* alloc(size_t size) {
        size += MSG_HEADER_SIZE;
        if (size >= m_free_size) {
            uint32_t read_idx_cache = m_read_idx.load(std::memory_order_relaxed);
            // rewind m_write_idx
            if(m_write_idx >= read_idx_cache) {
                //m_free_size = q_size - m_write_idx;
                //if (m_free_size <= size && read_idx_cache != 0) {
                if (read_idx_cache != 0) {
                    reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = 1;
                    // avoid m_write_idx update before flag the wrap around
                    m_write_idx = 0;
                    write_c += 1;
                    reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = 0;
                    m_free_size = read_idx_cache;
                }
            }
            else {
                m_free_size = read_idx_cache - m_write_idx;
            }
            if (m_free_size <= size){
                return nullptr;
            }
        }
        auto* ret = reinterpret_cast<MsgHeader*>(&buf[m_write_idx]);
        ret->size = size;
        return ret; 
    }

    void push() {
        // ensure writer finished writing
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_write_idx]);
        m_write_idx = m_write_idx + header->size;
        m_free_size = m_free_size - header->size;
        // put an empty header after msg to avoid reader corss the end
        // downside is that it may overwrite the last unread
        reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = 0;
    }

    const MsgHeader* front() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        //rewind
        if (header->size == 1) {
            m_read_idx = 0;
            read_c += 1;
            header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        }
        if (header->size == 0) return nullptr;
        return header;
    }

    void pop() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        m_read_idx.store(m_read_idx + header->size, std::memory_order_relaxed);
    }

    
    
//private:
uint32_t read_c;
uint32_t write_c;
    bool first = true;
    static constexpr uint32_t CNT = 16 * 1024;
    static constexpr uint32_t q_size = MSG_HEADER_SIZE * CNT ;
    alignas(64) char buf[q_size] = {};

    alignas(128) uint32_t m_write_idx {0};
    uint32_t m_free_size = q_size;
    alignas(128) std::atomic<uint32_t> m_read_idx {0};

};
}
}
