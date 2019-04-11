#ifndef __NOEXCEPT_H__
#define __NOEXCEPT_H__

#include <iterator>
#include <memory>
#include <new>

template <class T>
struct Construct
{
    void construct(T * p, size_t count) const noexcept
    {
        for (size_t i = 0; i < count; ++i)
            new(p + i) T();
    }

    void destruct(T * p, size_t count) const noexcept
    {
        for (size_t i = 0; i < count; ++i)
            (p + i)->~T();
    }

    void move(T * old_p, size_t old_size, T * new_p, size_t new_size) const noexcept
    {
        size_t i = 0;
        for (; i < old_size; ++i)
            new (new_p + i) T(std::move(old_p[i]));

        for (; i < new_size; ++i)
            new(new_p + i) T();
    }
};

template <class T>
struct NoConstruct
{
    void construct(T * /*p*/, size_t /*count*/) const noexcept
    {}

    void destruct(T * /*p*/, size_t /*count*/) const noexcept
    {}

    void move(T * old_p, size_t old_size, T * new_p, size_t /*new_size*/) const noexcept
    {
        memcpy(new_p, old_p, old_size * sizeof(T));
    }
};

template <class T, template <class> class Constructor>
struct NoRealloc
{
    void realloc(T * old_p, size_t old_size, T * new_p, size_t new_size, Constructor<T>& constructor)
    {
        constructor.destruct(old_p, old_size);
        constructor.construct(new_p, new_size);
    }
};

template <class T, template <class> class Constructor>
struct Realloc
{
    void realloc(T * old_p, size_t old_size, T * new_p, size_t new_size, Constructor<T>& constructor)
    {
        constructor.move(old_p, old_size, new_p, new_size);
        constructor.destruct(old_p, old_size);
    }
};

template <class T, template <class> class Constructor = NoConstruct, template <class, template <class> class> class Reallocator = NoRealloc>
class NoThrowMemoryStorage
{
public:
    T * get() const noexcept { return m_data; }

    NoThrowMemoryStorage(size_t size) noexcept
        : m_data((T*)malloc(size * sizeof(T)))
    {
        if (m_data != nullptr)
        {
            m_size = size;
            m_capacity = size;
            m_constructor.construct(m_data, m_size);
        }
        else
        {
            m_size = 0;
            m_capacity = 0;
        }
    }

    ~NoThrowMemoryStorage()
    {
        if (m_data != nullptr)
        {
            m_constructor.destruct(m_data, m_capacity);
            free(m_data);
        }  
    }

    bool resize(size_t size) noexcept
    {
        if (m_capacity >= size)
        {
            m_size = size;
            return true;
        }

        T * new_data = (T*)malloc(size * sizeof(T));
        if (new_data == nullptr)
        {
            if (m_data != nullptr)
            {
                m_constructor.destruct(m_data, m_size);
                free(m_data);
                m_size = 0;
                m_capacity = 0;
            }

            return false;
        }

        if (m_data != nullptr)
            m_reallocator.realloc(m_data, m_size, new_data, size, m_constructor);
        else
            m_constructor.construct(new_data, size);

        m_data = new_data;
        m_size = size;
        m_capacity = size;

        return true;
    }

    size_t size() const noexcept { return m_size; }

    operator bool() const noexcept { return (m_data != nullptr); }

protected:
    T *    m_data;
    size_t m_size;
    size_t m_capacity;

    Constructor<T> m_constructor;
    Reallocator<T, Constructor> m_reallocator;
};

class PtrStack
{
protected:
    NoThrowMemoryStorage<void*, NoConstruct, Realloc> m_storage;
    size_t m_top;
public:
    PtrStack(size_t capacity) noexcept : m_storage(capacity), m_top(0) {}
    bool pop(void *& p) noexcept
    {
        if (m_top == 0)
            return false;

        p = m_storage.get()[--m_top];
        return true;
    }

    bool push(void * p) noexcept
    {
        size_t size = m_storage.size();
        if (m_top >= size)
        {
            if (!m_storage.resize(size * 2))
                return false;
        }
            
        m_storage.get()[m_top++] = p;
        return true;
    }

    size_t size() const noexcept { return m_top; }
    size_t capacity() const noexcept { return m_storage.size(); }
    bool good() const noexcept { return (m_storage.get() != nullptr); }
};

template <class T>
class NoThrowList
{
public:
    struct Node
    {
        Node * prev;
        Node * next;
        T data;
    };

    NoThrowList() noexcept : m_head(nullptr), m_tail(nullptr), m_size(0), m_allocator_stack(1024 * 1024), m_blocks_stack(64 * 1024)
    {}

    class iterator
    {
    public:
        typedef T  value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef void difference_type;
        typedef std::bidirectional_iterator_tag iterator_category;

        iterator() noexcept : m_node(nullptr)
        {}

        explicit iterator(Node * node) noexcept : m_node(node)
        {}

        iterator& operator ++() noexcept
        {
            m_node = m_node->next;
            return *this;
        }

        iterator& operator --() noexcept
        {
            m_node = m_node->prev;
            return *this;
        }

        bool operator == (const iterator& rhs) noexcept
        {
            return (m_node == rhs.m_node);
        }

        bool operator != (const iterator& rhs) noexcept
        {
            return !(*this == rhs);
        }

        Node * get() const noexcept
        {
            return m_node;
        }

        T& operator * () const noexcept 
        { 
            return m_node->data;
        }

        T* operator ->() const noexcept
        {
            return &m_node->data;
        }

        bool empty() const noexcept
        {
            return (m_node == nullptr);
        }

    protected:
        Node * m_node;
    };

    iterator end() const noexcept
    {
        return iterator();
    }

    iterator begin() const noexcept
    {
        return iterator(m_head);
    }

    bool empty() const noexcept
    {
        return (m_head == nullptr);
    }

    size_t size()
    {
        return m_size;
    }

    template <class... Args>
    T* emplace_back(Args... args) noexcept
    {
        Node * new_node = allocate();
        if (new_node == nullptr)
            return nullptr;

        new (&new_node->data) T(std::forward<Args>(args)...);
        new_node->next = nullptr;

        if (m_tail == nullptr)
            m_head = new_node;
        else
            m_tail->next = new_node;
            
        new_node->prev = m_tail;
        m_tail = new_node;
        ++m_size;

        return &new_node->data;
    }

    template <class... Args>
    T* emplace_front(Args... args) noexcept
    {
        Node * new_node = allocate();
        if (new_node == nullptr)
            return nullptr;

        new (&new_node->data) T(std::forward<Args>(args)...);
        new_node->prev = nullptr;

        if (m_head == nullptr)
            m_tail = new_node;
        else
            m_head->prev = new_node;

        new_node->next = m_head;
        m_head = new_node;
        ++m_size;

        return &new_node->data;
    }

    iterator erase(const iterator& it) noexcept
    {
        Node * node = it.get();
        Node * const prev = node->prev,
             * const next = node->next;

        if (prev == nullptr)
        {
            m_head = m_head->next;
            if (m_head != nullptr)
                m_head->prev = nullptr;
        }   
        else
            prev->next = next;

        if (next == nullptr)
        {
            m_tail = m_tail->prev;
            if (m_tail != nullptr)
                m_tail->next = nullptr;
        }  
        else
            next->prev = prev;

        node->data.T::~T();
        deallocate(node);
        --m_size;

        return iterator(next);
    }

    void pop_back()
    {
        if (empty())
            return;

        Node * const prev = m_tail->prev;
        if (prev == nullptr)
            m_head = nullptr;

        m_tail->data.T::~T();
        deallocate(m_tail);

        m_tail = prev;
        if (m_tail != nullptr)
            m_tail->next = nullptr;
        --m_size;
    }

    T * insert(iterator where, const T& data)
    {
        Node * node = where.get();
        if (node == nullptr)
            return emplace_back(data);
        else if (node == m_head)
            return emplace_front(data);
        else
        {
            Node * new_node = allocate();
            if (new_node == nullptr)
                return nullptr;

            Node * const prev = node->prev;
            new (&new_node->data) T(data);
            new_node->prev = prev;
            prev->next = new_node;
            new_node->next = node;
            node->prev = new_node;
            ++m_size;

            return &new_node->data;
        }
    }

    template <class Comaparer>
    void sort(Comaparer comp)
    {
        qsort(comp, m_head, nullptr, &m_head);
        Node * prev = nullptr;
        for (Node * cur = m_head; cur != nullptr; cur = cur->next)
        {
            cur->prev = prev;
            prev = cur;
        }

        m_tail = prev;
    }

    ~NoThrowList() noexcept
    {
        Node * p = m_head;
        while (p != nullptr)
        {
            Node * next = p->next;
            p->data.T::~T();
            //deallocate(p);
            p = next;
        }

        void * block = nullptr;
        while(m_blocks_stack.pop(block))
            free(block);

        m_size = 0;
        m_head = nullptr;
        m_tail = nullptr;
    }

protected:
    Node * m_head;
    Node * m_tail;
    size_t m_size;
    PtrStack m_allocator_stack;
    PtrStack m_blocks_stack;

    Node * allocate()
    {
        if (!m_allocator_stack.good())
            return nullptr;

        void * p = nullptr;
        if (m_allocator_stack.pop(p))
            return (Node*)p;

        if (!m_blocks_stack.good())
            return nullptr;

        size_t count = m_allocator_stack.capacity();
        Node * block = (Node*)malloc(sizeof(Node) * count);
        if (!m_blocks_stack.push(block))
        {
            free(block);
            return nullptr;
        }

        for (size_t i = 1; i < count; ++i)
            m_allocator_stack.push(block + i);

        return block;
    }
    void deallocate(Node * p)
    {
        m_allocator_stack.push(p);
    }

    template <class Comaparer>
    void qsort(Comaparer& comp, Node * hd, Node * tl, Node ** rtn)
    {
        int nlo, nhi;
        Node *lo, *hi, *q, *p;

        /* Invariant:  Return head sorted with `tl' appended. */
        while (hd != nullptr)
        {
            nlo = nhi = 0;
            lo = hi = nullptr;
            q = hd;
            p = hd->next;

            /* Partition and count sizes. */
            while (p != nullptr) 
            {
                q = p->next;
                if (!comp(hd->data, p->data))
                {
                    p->next = lo;
                    lo = p;
                    ++nlo;
                }
                else 
                {
                    p->next = hi;
                    hi = p;
                    ++nhi;
                }
                p = q;
            }

            /* Recurse to establish invariant for sublists of hd,
               choosing shortest list first to limit stack. */
            if (nlo < nhi) 
            {
                qsort(comp, lo, hd, rtn);
                rtn = &hd->next;
                hd = hi;        /* Eliminated tail-recursive call. */
            }
            else 
            {
                qsort(comp, hi, tl, &hd->next);
                tl = hd;
                hd = lo;        /* Eliminated tail-recursive call. */
            }
        }
        /* Base case of recurrence. Invariant is easy here. */
        *rtn = tl;
    }
};

#endif // __NOEXCEPT_H__
