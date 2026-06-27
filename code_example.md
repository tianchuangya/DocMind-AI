# Python 列表示例

## 正确的代码块格式

### 1. 使用三反引号（推荐）

```python
# 这是正确的代码块格式
fruit_list = ["apple", "banana", "orange"]
print(fruit_list[1])  # banana
```

### 2. 带语言标识

```python
# 创建列表
num_list = [1, 2, 3, 4, 5]

# 列表推导式
even_list = [x for x in range(1, 11) if x % 2 == 0]
print(even_list)  # [2, 4, 6, 8, 10]
```

### 3. 二维列表示例

```python
# 创建 3x3 二维列表
matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
]

# 访问元素
print(matrix[1][2])  # 6
```

### 4. 列表操作

```python
# 添加元素
num_list = [1, 2, 3]
num_list.append(4)
print(num_list)  # [1, 2, 3, 4]

# 删除元素
num_list.pop(1)
print(num_list)  # [1, 3, 4]

# 排序
num_list.sort(reverse=True)
print(num_list)  # [4, 3, 1]
```

## 表格示例

| 方法 | 说明 | 示例 |
|------|------|------|
| `append(x)` | 末尾添加 | `list.append(1)` |
| `pop(i)` | 删除索引i | `list.pop(0)` |
| `sort()` | 排序 | `list.sort()` |

## 任务列表示例

- [x] 创建列表
- [x] 添加元素
- [ ] 删除元素
- [ ] 排序列表
