#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {

        void SimpleLRU::push_front(const std::string &key, const std::string &value) {
            std::unique_ptr<lru_node> node = std::unique_ptr<lru_node>(new lru_node(key, value));
            if (_lru_head == nullptr) {
                _lru_head = std::move(node);
                _lru_tail = _lru_head.get();
            } else {
                _lru_head->prev = node.get();
                node->next = std::move(_lru_head);
                _lru_head = std::move(node);
            }
            // Add to map
            _lru_index.insert(std::pair<std::reference_wrapper<const std::string>,
                    std::reference_wrapper<lru_node>>(_lru_head->key, *_lru_head));

            //  Update curr_size
            _cur_size += _lru_head->key.size() + _lru_head->value.size();
        }

        bool SimpleLRU::pop_back() {
            if (_lru_head == nullptr) {
                return false;
            }

            // Delete from map
            _lru_index.erase(_lru_tail->key);

            //Delete form list
            _cur_size -= _lru_tail->value.size() + _lru_tail->key.size();
            if (_lru_head.get() == _lru_tail) {
                _lru_head.reset();
            } else {
                _lru_tail = _lru_tail->prev;
                _lru_tail->next.reset();
            }
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key, const std::string &value) {
            bool is_absent = true;
            auto it = _lru_index.find(key);
            if (it != _lru_index.end()) {
                is_absent = false;
            }
            if (is_absent) {
//                try {
//                    PutIfAbsent(key, value);
//                } catch (const std::exception &exception) {
//                    throw exception;
//                }

                bool is_poped = true;
                bool overflow = _cur_size + value.size() + key.size() > _max_size;
                while (overflow && is_poped) {
                    is_poped = pop_back();
                    overflow = _cur_size + value.size() + key.size() > _max_size;
                }
                if (!overflow) {
                    push_front(key, value);
                } else {
                    throw std::overflow_error(key);
                }
            } else {
//                try {
//                    Set(key, value);
//                } catch (const std::exception &exception) {
//                    throw exception;
//                }

                auto curr_node = &((*it).second.get());

                // Moving to the head;
                if (curr_node != _lru_head.get()) {
                    if (curr_node == _lru_tail) {
                        _lru_tail = curr_node->prev;
                        _lru_tail->next.reset();
                    } else {
                        curr_node->next->prev = curr_node->prev;
                        curr_node->prev->next = std::move(curr_node->next);
                        delete (curr_node);
                    }
                }

                // Update value in list
                _cur_size -= curr_node->value.size();
                bool overflow = _cur_size + value.size() > _max_size;
                bool is_poped = !(_lru_head == nullptr);
                while (overflow && is_poped) {
                    is_poped = pop_back();
                    overflow = _cur_size + value.size() > _max_size;
                }
                if (!overflow) {
                    push_front(key, value);
                } else {
                    throw std::overflow_error(key);
                }

                // Update value in map
                (*it).second = std::reference_wrapper<lru_node>(*_lru_head);
            }
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
            auto it = _lru_index.find(key);
            if (it != _lru_index.end()) {
                return false;
            }
            bool is_poped = true;
            bool overflow = _cur_size + value.size() + key.size() > _max_size;
            while (overflow && is_poped) {
                is_poped = pop_back();
                overflow = _cur_size + value.size() + key.size() > _max_size;
            }
            if (!overflow) {
                push_front(key, value);
            } else {
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
            if (curr_node != _lru_head.get()) {
                if (curr_node == _lru_tail) {
                    _lru_tail = curr_node->prev;
                    _lru_tail->next.reset();
                } else {
                    curr_node->next->prev = curr_node->prev;
                    curr_node->prev->next = std::move(curr_node->next);
                    delete (curr_node);
                }
            }

            // Update value in list
            _cur_size -= curr_node->value.size();
            bool overflow = _cur_size + value.size() > _max_size;
            bool is_poped = !(_lru_head == nullptr);
            while (overflow && is_poped) {
                is_poped = pop_back();
                overflow = _cur_size + value.size() > _max_size;
            }
            if (!overflow) {
                push_front(key, value);
            } else {
                throw std::overflow_error(key);
            }

            // Update value in map
            (*it).second = std::reference_wrapper<lru_node>(*_lru_head);

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
            if (_lru_head.get() == _lru_tail) {
                _lru_head.reset();
                _lru_tail = nullptr;
            } else if (del_node == _lru_head.get()) {
                _lru_head = std::move(del_node->next);
                delete (_lru_head->prev);
                _lru_head->prev = nullptr;
            } else if (del_node == _lru_tail) {
                _lru_tail = del_node->prev;
                _lru_tail->next.reset();
            } else {
                del_node->next->prev = del_node->prev;
                del_node->prev->next = std::move(del_node->next);
                delete (del_node);
            }
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key, std::string &value) const {
            auto it = _lru_index.find(key);
            if (it != _lru_index.end()) {
                value = (*it).second.get().value;
                return true;
            }
            return false;

        }

    } // namespace Backend
} // namespace Afina
