# cecs

Это моя реализация ECS, с архетипным подходом, в котором select запросы используют constexpr индекс.

## Примеры:

Большинство примеров можно посмотреть в тестах. Но вот выдержка из них

### Создание сущности с компонентами

```c++
NCecs::TWorld<
    NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
    world;

world.create<int, char>(42, '0');
world.create<int>(10);
```

### Select сущностей по компонентам

```c++
NCecs::TWorld<
    NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
    world;

world.create<int, char>(42, '0');

int res = 0;
auto visitor = [&res](auto entity) {
    res += entity.get<int>();
};
world.select<int>(visitor);

assert(res == 42);
```