    bool Contains(x) {
        curr = head
        while (curr->key < x) {
            curr = curr->next
        }
        return curr->key == x && !curr->marked
    }

В этой реализации выбрать чтение флага `marked` в качестве точки линеаризации нельзя, так как это флаг читается не во всех вызовах `Contains`, так как в случае `curr->key != x` правая часть выражения не будет вычисляться.