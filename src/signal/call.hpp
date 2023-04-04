// This file is derived from SomeProject
// Original Author: BusyStudent(https://github.com/BusyStudent/Btk-ng)
// Original License: MIT License

// This file is part of MyProject
// Author: llhsdmd(llhsdmd@qq.com)
// Copyright (c) 2021 Your Company

/*
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so...
*/
#pragma once

#include <type_traits>
#include <utility>

//For call
template<class Callable,class ...Args>
auto _Call(Callable &&callable,Args &&...args) -> 
    std::invoke_result_t<Callable,Args...>{
    
    return callable(std::forward<Args>(args)...);
}

template<class RetT,class Class,class ...FnArgs,class ...Args>
auto _Call(RetT (Class::*method)(FnArgs ...),Class *object,Args &&...args){

    return (object->*method)(std::forward<Args>(args)...);
}
template<class RetT,class Class,class ...FnArgs,class ...Args>
auto _Call(RetT (Class::*method)(FnArgs ...) const,const Class *object,Args &&...args){

    return (object->*method)(std::forward<Args>(args)...);
}
