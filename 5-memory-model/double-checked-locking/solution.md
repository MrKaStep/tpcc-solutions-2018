# Double-checked locking

### Какие чтения/записи упорядочивать?

Упорядочены должны быть вызов `create_` и разыменование `*curr_ptr`.  

### Гарантия упорядочивания?

Возникновение happens-before между вызовом `create_` и `*curr_ptr`
достигается следующей цепочкой:

`curr_ptr = create_();` --*program-order*-- `ptr_to_value_.store(curr_ptr, std::memory_order_release)`
--*syncronizes-with*--`curr_ptr = ptr_to_value_.load(std::memory_order_acquire)`
--*program-order*--`return *curr_ptr`

### Почему нельзя слабее?

Единственное возможное ослабление - замена всех порядков на `relaxed`, что
приведёт к потере стрелки `syncronizes-with` и к возможному разыменовынию
некорректного укаателя в другом потоке. 
