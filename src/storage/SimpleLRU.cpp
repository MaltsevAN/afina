#include "SimpleLRU.h"

namespace Afina {
namespace Backend {
size_t SimpleLRU::get_max_size() const  {
    return _max_size;
}
size_t SimpleLRU::get_cur_size() const   {
    return _cur_size;
}
void SimpleLRU::print_map() const {
    std::cout << "!!!!!!!!!!MAP!!!!!!!!!!!!!!!" << std::endl;
    for(auto item: _lru_index)
    {
        std::cout << "key = " << item.first.get()
                    << "__value = " << item.second.get().value << std::endl;
    }
    std::cout << "__________END_______________" << std::endl;
}

void SimpleLRU::print_list() const {
    std::cout << "!!!!!!!!!!LIST!!!!!!!!!!!!!!!" << std::endl;
    auto it = _lru_head.get();
    while(it != nullptr)
    {
        std::cout << "KEY = " << it->key << std::endl;
        std::cout << "VALUE = " << it->value << std::endl;
        it = it->next.get();
    }
    std::cout << "===========END=================" << std::endl;
}
void SimpleLRU::push_front(const std::string &key, const std::string &value)
{
//    std::cout << "void SimpleLRU::push_front(const std::string &key, const std::string &value)" << std::endl;
//    std::cout << "key = " << key << "___value = " << value <<std::endl;
    std::unique_ptr<lru_node> node = std::unique_ptr<lru_node>(new lru_node(key, value));
    if (_lru_head == nullptr)
    {
        _lru_head = std::move(node);
        _lru_tail = _lru_head.get();
    } else
    {
        _lru_head->prev = node.get();
        node->next = std::move(_lru_head);
        _lru_head = std::move(node);
    }
    // Add to map
    _lru_index.insert(std::pair<std::reference_wrapper<const std::string>,
            std::reference_wrapper<lru_node>>(_lru_head->key, *_lru_head));

    //  Update curr_size
    _cur_size += _lru_head->key.size() + _lru_head->value.size();
//    print_map();
//    print_list();
}
bool SimpleLRU::pop_back()
{
    if (_lru_head == nullptr)
    {
        return false;
    }

    // Delete from map
    _lru_index.erase(_lru_tail->key);

    //Delete form list
    _cur_size -= _lru_tail->value.size() + _lru_tail->key.size();
    if (_lru_head.get() == _lru_tail)
    {
        _lru_head.reset();
    } else
    {
        _lru_tail = _lru_tail->prev;
        _lru_tail->next.reset();
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value)
{
//    std::cout << "======================Put==============================" << std::endl;
//    print_map();
//    print_list();
//    std::cout << "PUT = key = " << key << "___value = " << value <<std::endl;
    bool is_absent = true;
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        is_absent = false;
    }
    if (is_absent)
    {
//        std::cout << "IS_ABSENT" << std::endl;
        try
        {
            PutIfAbsent(key, value);
        } catch(const std::exception& exception)
        {
//            throw exception;
//            std::cout << "EXCEPTION" << std::endl;
            return false;
        }
    } else
    {
//        std::cout << "is_abset" << is_absent << "   it = " << (*it).first.get() << std::endl;
//        std::cout << "NO_ABSENT" << std::endl;
        try
        {
            Set(key, value);
        } catch(const std::exception& exception)
        {
//            std::cout << "EXCEPTION" << std::endl;
//            throw exception;
            return false;
        }
    }
//    std::cout << "++++++++++++++AFTER PUT++++++++" << std::endl;
//    print_map();
//    print_list();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        return false;
    }
//    std::cout << "bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {" << std::endl;
    bool is_poped = true;
    bool overflow = _cur_size + value.size() + key.size() > _max_size;
    while (overflow && is_poped)
    {
//        std::cout << "############POOOOOPING########" << std::endl;
        is_poped = pop_back();
        overflow = _cur_size + value.size() + key.size()  > _max_size;
    }
    if (!overflow)
    {
        push_front(key, value);
    } else
    {
//        std::cout << "EXCEPTION" << std::endl;
        throw std::overflow_error(key);
    }
}


// See MapBasedGlobalLockImpl.h

// First version (with moving)
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }
    auto curr_node = &((*it).second.get());

    // Moving to the head;
    if (curr_node != _lru_head.get())
    {
        if (curr_node == _lru_tail)
        {
            _lru_tail = curr_node->prev;
            _lru_tail->next.reset();
        } else {
            curr_node->next->prev = curr_node->prev;
            curr_node->prev->next = std::move(curr_node->next);
            free(curr_node);
        }
    }

    // Update value in list
    _cur_size -= curr_node->value.size();
    bool overflow = _cur_size + value.size() > _max_size;
    bool is_poped = !(_lru_head == nullptr);
    while (overflow && is_poped)
    {
        is_poped = pop_back();
        overflow = _cur_size + value.size()  > _max_size;
    }
    if (!overflow)
    {
        push_front(key, value);
    } else {
//        std::cout << "EXCEPTION" << std::endl;
        throw std::overflow_error(key);
    }

    // Update value in map
     (*it).second = std::reference_wrapper<lru_node >(*_lru_head);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }
    auto del_node = &((*it).second.get());

    // Delete from map
    _lru_index.erase(it);


    // Delete from list
    _cur_size -= del_node->value.size() + del_node->key.size();
    if (_lru_head.get() == _lru_tail)
    {
        _lru_head.reset();
    } else if (del_node == _lru_head.get())
    {
        _lru_head = std::move(del_node->next);
        free(_lru_head->prev);
        _lru_head->prev = nullptr;
    } else if (del_node == _lru_tail)
    {
        _lru_tail = del_node->prev;
        _lru_tail->next.reset();
    } else
    {
        del_node->next->prev = del_node->prev;
        del_node->prev->next = std::move(del_node->next);
        free(del_node);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) const
{
//    std::cout << "GET with key = " << key << "\n" << std::endl;
    auto it = _lru_index.find(key);
//    print_list();
//    print_map();
//    std::cout << "CHEK::" << (it != _lru_index.end())  << "\n" <<  std::endl;
    if (it != _lru_index.end())
    {
        value = (*it).second.get().value;
//        std::cout << value << std::endl;
        return true;
    }
    return false;

}

} // namespace Backend
} // namespace Afina
