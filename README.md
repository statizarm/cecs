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
world.select<int>().run(visitor);

assert(res == 42);
```

### Batch обработка результата выбора

```c++
static constexpr std::size_t kSize = 1 << 20;
NCecs::TWorld<NCecs::TTypeList<
    NCecs::TTypeList<std::size_t>,
    NCecs::TTypeList<std::size_t, std::string>>>
    world;

for (std::size_t i = 0; i < kSize; ++i) {
    if (i & 1) {
        world.create<std::size_t, std::string>(i, "hello");
    } else {
        world.create<std::size_t>(i);
    }
}

world.commit();

auto [counter, sum] = world.select<std::size_t>().run_batched(
    [&](auto& subworld) -> std::pair<std::size_t, std::size_t> {
        std::size_t sum     = 0;
        std::size_t counter = 0;
        for (auto entity : subworld) {
            ++counter;
            sum += entity.template get<std::size_t>();
            if (entity.template has<std::string>()) {
                EXPECT_EQ(entity.template get<std::string>(), "hello");
            }
        }
        return std::make_pair(counter, sum);
    }
);
```