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
        m_curt_write_size = size + MSG_HEADER_SIZE;
        if (m_curt_write_size >= m_free_size) {
            uint32_t read_idx_cache = m_read_idx.load(std::memory_order_relaxed);
            // rewind m_write_idx
            if(m_write_idx >= read_idx_cache) {
                m_free_size = q_size - m_write_idx;  //? why
                if (m_free_size <= m_curt_write_size && read_idx_cache != 0) {  // why
                    // every time we set next header to 0 first like what we do in push()
                    reinterpret_cast<MsgHeader*>(&buf[0])->size = 0;
                    std::atomic_thread_fence(std::memory_order_seq_cst);

                    reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = 1;
                    // avoid m_write_idx update before flag the wrap around
                    m_write_idx = 0;
                    write_c += 1;
                    m_free_size = read_idx_cache;
                }
            }
            else {
                m_free_size = read_idx_cache - m_write_idx;
            }
            if (m_free_size <= m_curt_write_size){
                return nullptr;
            }
        }
        return  reinterpret_cast<MsgHeader*>(&buf[m_write_idx]);
    }

    // We write 0 to next header first and flush the cache. By doing so, we avoid one scenario
    // that because of reoder, curt header's size is assigned before next header's, and exactly 
    // after curt header's assignment and before next's, reader starts to read and continuing on
    // old values without a sign to stop.
    void push() {
        reinterpret_cast<MsgHeader*>(&buf[m_write_idx + m_curt_write_size])->size = 0;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        // std::atomic_thread_fence(std::memory_order_release);

        reinterpret_cast<MsgHeader*>(&buf[m_write_idx])->size = m_curt_write_size;
        m_write_idx += m_curt_write_size;
        m_free_size -= m_curt_write_size;
    }

    const MsgHeader* front() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        //rewind
        if (header->size == 1) {
            m_read_idx = 0;
            read_c += 1;
            header = reinterpret_cast<MsgHeader*>(&buf[0]);
        }
        if (header->size == 0) return nullptr;
        return header;
    }

    void pop() {
        auto* header = reinterpret_cast<MsgHeader*>(&buf[m_read_idx]);
        m_read_idx.store(m_read_idx + header->size, std::memory_order_seq_cst);
    }

    
    
//private:
uint32_t read_c;
uint32_t write_c;
    static constexpr uint32_t CNT = 16 * 1024;
    static constexpr uint32_t q_size = MSG_HEADER_SIZE * CNT ;
    alignas(64) char buf[q_size] = {};

    alignas(128) uint32_t m_write_idx {0};
    uint32_t m_free_size = q_size;
    uint32_t m_curt_write_size{0};
    alignas(128) std::atomic<uint32_t> m_read_idx{0};

};
}
}
