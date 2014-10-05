#include "signal_trace.h"

size_t signal_trace::length() const
{
    if (blocks.empty())
        return 0;

    std::pair<size_t, block_info> const & bi = *blocks.rbegin();
    return bi.first + bi.second.block_length * bi.second.repeat_count;
}

bool signal_trace::sample(size_t index) const
{
    std::map<size_t, block_info>::const_iterator it = blocks.upper_bound(index);
    Q_ASSERT(it != blocks.begin());
    --it;

    index -= it->first;
    index %= it->second.block_length;

    return data[it->second.block_offset + index];
}

static void reduce_block(std::pair<bool, bool> & res, std::vector<bool> const & v, size_t first, size_t last)
{
    for (; (!res.first || !res.second) && first != last; ++first)
    {
        if (v[first])
            res.second = true;
        else
            res.first = true;
    }
}

std::pair<bool, bool> signal_trace::multisample(size_t first, size_t last) const
{
    sample_ptr first_ptr = this->get_sample_ptr(first);
    sample_ptr last_ptr = this->get_sample_ptr(last);

    std::pair<bool, bool> res(false, false);

    if (first_ptr.block_it == last_ptr.block_it
        && first_ptr.repeat_index == last_ptr.repeat_index)
    {
        reduce_block(res, data,
            first_ptr.block_it->second.block_offset + first_ptr.repeat_offset,
            last_ptr.block_it->second.block_offset + last_ptr.repeat_offset);
        return res;
    }

    std::map<size_t, block_info>::const_iterator first_complete = first_ptr.block_it;
    std::map<size_t, block_info>::const_iterator last_complete = last_ptr.block_it;
    if (first_ptr.last_repeat)
    {
        // Handle partial block
        reduce_block(res, data, first_ptr.block_it->second.block_offset + first_ptr.repeat_offset,
            first_ptr.block_it->second.block_offset + first_ptr.block_it->second.block_length);
        ++first_complete;
    }

    if (last_ptr.first_repeat)
    {
        reduce_block(res, data, last_ptr.block_it->second.block_offset,
            last_ptr.block_it->second.block_offset + last_ptr.repeat_offset);
        if (first_complete == last_complete)
            return res;
        --last_complete;
    }

    reduce_block(res, data, first_complete->second.block_offset, last_complete->second.block_offset + last_complete->second.block_length);
    return res;
}

signal_trace::sample_ptr signal_trace::get_sample_ptr(size_t sample) const
{
    sample_ptr res;
    res.block_it = blocks.upper_bound(sample);
    Q_ASSERT(res.block_it != blocks.begin());
    --res.block_it;

    res.block_sample_offset = sample - res.block_it->first;
    res.repeat_index = res.block_sample_offset / res.block_it->second.block_length;
    res.repeat_offset = res.block_sample_offset % res.block_it->second.block_length;
    res.data_offset = res.block_it->second.block_offset + res.repeat_offset;

    res.first_repeat = res.repeat_index == 0;
    res.last_repeat = res.repeat_index == res.block_it->second.repeat_count - 1;
    return res;
}
