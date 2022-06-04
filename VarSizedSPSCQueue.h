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
        uint64_t read_idx_cache = m_read_idx.load(std::memory_order_relaxed);
        // ^ should I do it with volatile?
        if(m_write_idx >= read_idx_cache) {
            size_t free_size = q_size - m_write_idx;
            if(free_size < size) {
                //wrap around
                // set header.size to 1 so reader knows to rewind
                reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = 1;
                m_write_idx.store(0, std::memory_order_relaxed);
                reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = 0;
                free_size = read_idx_cache - m_write_idx;
                if(free_size < size) {
                    return nullptr;
                }
            }
            auto* ret = reinterpret_cast<MsgHeader*>(&buf[m_write_idx]);
            ret->size = size;
            return ret; 
        }
        else {
            if(m_write_idx + size < read_idx_cache) {
                auto* ret = reinterpret_cast<MsgHeader*>(&buf[m_write_idx]);
                ret->size = size;
                return ret;
            }
            else {
                return nullptr;
            }
        }
    }

    void push() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_write_idx]);
        m_write_idx.store(m_write_idx + header->size, std::memory_order_relaxed);
    }

    const MsgHeader* front() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        //rewind
        if (header->size == 1) {
            m_read_idx = 0;
            header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        }
        if (header->size == 0) return nullptr;
        return header;
    }

    void pop() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        m_read_idx.store(m_read_idx + header->size, std::memory_order_relaxed);
    }

    
    
private:
    static constexpr uint32_t CNT = 16 * 1024;
    static constexpr uint32_t q_size = MSG_HEADER_SIZE * CNT;
    alignas(64) char buf[q_size] = {};

    alignas(128) std::atomic<uint64_t> m_write_idx {0};
    alignas(128) std::atomic<uint64_t> m_read_idx {0};

};
}
}