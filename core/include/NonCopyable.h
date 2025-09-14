#pragma once

/**
 * 继承此类的派生类将：
 * - 自动禁用拷贝构造函数和拷贝赋值运算符，防止对象被复制；
 * - 允许默认构造、析构；
 * - 移动构造和移动赋值默认支持（如需要的话，可以取消注释）。
 */
class NonCopyable {
public:
    // 删除拷贝构造函数，禁止拷贝构造
    NonCopyable(const NonCopyable& other) = delete;

    // 删除拷贝赋值运算符，禁止拷贝赋值
    NonCopyable& operator=(const NonCopyable& other) = delete;

    // （可选）默认移动构造函数，允许移动语义
    NonCopyable(NonCopyable&& other) noexcept = default;

    // （可选）默认移动赋值运算符，允许移动语义
    NonCopyable& operator=(NonCopyable&& other) = default;

protected:
    // 默认构造函数，允许子类构造
    NonCopyable() = default;

    // 默认析构函数，允许子类析构
    ~NonCopyable() = default;
};
