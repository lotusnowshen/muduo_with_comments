#ifndef MUDUO_BASE_COPYABLE_H
#define MUDUO_BASE_COPYABLE_H

namespace muduo
{

/// A tag class emphasises the objects are copyable.
/// The empty base class optimization applies.
/// Any derived class of copyable should be a value type.
//标签类，继承该类的class具有值语义，应该使用私有继承
class copyable
{
};

};

#endif  // MUDUO_BASE_COPYABLE_H
