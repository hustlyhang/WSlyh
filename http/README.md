每个连接都会绑定一个http对象，同时每个http对象都要绑定一个timer，以便于剔除长时间不活跃的连接
处理http请求准备调用http-parse      github地址https://github.com/nodejs/http-parser
准备以堆的形式来剔除超时连接


这个类 相当于一个task放入到线程池中


这个类需要实现处理http请求连接，
写http回送请求

还要在数据库中查询


时间堆的实现
需要一个结构体来保存用户的数据
    1、绑定socket
    2、绑定一个定时器
    3、保存连接的地址
实现一个timer类
    1、保存此timer的过期时间
    2、绑定用户数据
    3、需要实现一个函数来更新过期时间
    4、回调函数，如果过期了需要调用此函数来剔除这个定时器，同时处理用户数据
小根堆实现高性能定时器
    1、用数组来模拟堆
    2、向下调整
    3、添加timer、删除timr
    4、实现一个定时函数tick，用来删除堆中的超时连接
    5、动态扩容，如果数组容量不够，就动态得扩增容量