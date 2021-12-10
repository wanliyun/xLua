﻿/*!
 Area Of Interest In Game Developing
 Copyright Jerryzhou@outlook.com
 Licence: Apache 2.0
 
 Project: https://github.com/JerryZhou/aoi.git
 Purpose: Resolve the AOI problem in game developing
    with high run fps
    with minimal memory cost,
 
  Please see examples for more details.
 */

#ifndef __AOI_H_
#define __AOI_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define snprintf _snprintf
typedef _int64 int64_t;
#else
#include <stdbool.h>
#include <inttypes.h>
#include <sys/time.h>
#endif

/*****
 * AOI: (Area Of Interesting)
 * 1. 适用于大批量的对象管理和查询
 * 2. 采用四叉树来进行管理，可以通过少量改动支持3D
 * 3. 动态节点回收管理，减少内存, 节点的数量不会随着四叉树的层次变得不可控，与对象数量成线性关系
 * 3. 缓存搜索上下文，减少无效搜索，对于零变更的区域，搜索会快速返回
 * 4. 整个搜索过程，支持自定义过滤器进行单元过滤
 * 5. AOI 支持对象的内存缓冲区管理
 * 6. 线程不安全
 * 7. 支持单位半径，不同单位可以定义不同的半径(开启半径感知，会损失一部分性能)
 * 8. 只提供了搜索结构，不同单位的视野区域可以定义不一样
 * 
 * 提高内存访问，建议启用 iimeta (1)
 * 如果不需要单位半径感知 建议关闭 iiradius (0)
 */

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif
    
/* 是否启动元信息： 记录类型对象的内存使用情况, 并加一层对象的内存缓冲区 */
#define iimeta (1)
    
/* 是否启动单位半径支持 */
#define iiradius (1)

/* 常用布尔宏 */
#define iiyes 1
#define iiok 1
#define iino 0
    
/* 条件检查，不带assert */
#define icheck(con) do { if(!(con)) return ; } while(0)
#define icheckret(con, ret) do { if(!(con)) return ret; } while(0)

/* 节点查找行为 */
typedef enum EnumFindBehavior {
    /* 精确查找 */
    EnumFindBehaviorAccurate,
    /* 模糊查找 */
    EnumFindBehaviorFuzzy,
}EnumFindBehavior;

/* 获取当前系统的微秒数 */
int64_t igetcurmicro();
/* 获取当前系统的毫秒数 */
int64_t igetcurtick();
/* 获取系统的下一个唯一的事件微秒数 */
int64_t igetnextmicro();

/* 精度 */
typedef double ireal;

/* 编号 */
typedef int64_t iid;


/*************************************************************/
/* ipos                                                      */
/*************************************************************/

/* 坐标 */
typedef struct ipos {
    ireal x, y;
}ipos;
    
/* 计算距离的平方 */
ireal idistancepow2(const ipos *p, const ipos *t);
    
/*************************************************************/
/* ivec2                                                     */
/*************************************************************/

/* 向量,  完善基本的数学方法:  
 *  加法 ; 减法 ; 乘法 ; 除法 ; 点积(内积) ; 乘积(外积) ; 长度
 * */
typedef struct ivec2 {
    union {
        ireal values[2];
        struct {
            ireal x, y;
        } v;
    } u;
}ivec2;

/* 两点相减得到向量 */
ivec2 ivec2subtractpoint(const ipos *p0, const ipos *p1);

/* 加法*/
ivec2 ivec2add(const ivec2 *l, const ivec2 *r);

/* 减法 */
ivec2 ivec2subtract(const ivec2 *l, const ivec2 *r);

/* 乘法 */
ivec2 ivec2multipy(const ivec2 *l, const ireal a);

/* 点积 */
ireal ivec2dot(const ivec2 *l, const ivec2 *r);

/* 乘积 : 二维向量不存在叉积 
 * ivec2 ivec2cross(const ivec2 *l, const ivec2 *r);
 * */

/* 长度的平方 */
ireal ivec2lengthsqr(const ivec2 *l);

/* 长度 */
ireal ivec2length(const ivec2 *l);

/* 绝对值 */
ivec2 ivec2abs(const ivec2* l);

/* 平行分量, 确保 r 已经归一化 */
ivec2 ivec2parallel(const ivec2 *l, const ivec2 *r); 

/* 垂直分量, 确保 r 已经归一化 */
ivec2 ivec2perpendicular(const ivec2 *l, const ivec2 *r); 

/*************************************************************/
/* ivec3                                                     */
/*************************************************************/

/* 向量 完善基本的数学方法 */
typedef struct ivec3 {
    union {
        ireal values[3];
        struct {
            ireal x, y, z;
        }v;
    }u;
}ivec3;

/* 两点相减得到向量 */
/* ivec3 ivec3subtractpoint(const ipos *p0, const ipos *p1);*/

/* 加法*/
ivec3 ivec3add(const ivec3 *l, const ivec3 *r);

/* 减法 */
ivec3 ivec3subtract(const ivec3 *l, const ivec3 *r);

/* 乘法 */
ivec3 ivec3multipy(const ivec3 *l, const ireal a);

/* 点积 */
ireal ivec3dot(const ivec3 *l, const ivec3 *r);

/* 乘积 */ 
ivec3 ivec3cross(const ivec3 *l, const ivec3 *r);

/* 长度的平方 */
ireal ivec3lengthsqr(const ivec3 *l);

/* 长度 */
ireal ivec3length(const ivec3 *l);

/* 绝对值 */
ivec3 ivec3abs(const ivec3* l);

/* 归一*/
ivec3 ivec3normalize(const ivec3 *l);

/* 平行分量, 确保 r 已经归一化 */
ivec3 ivec3parallel(const ivec3 *l, const ivec3 *r);

/* 垂直分量, 确保 r 已经归一化 */
ivec3 ivec3perpendicular(const ivec3 *l, const ivec3 *r);

    
/*************************************************************/
/* isize                                                     */
/*************************************************************/

/* 大小 */
typedef struct isize {
    ireal w, h;
}isize;

/*************************************************************/
/* irect                                                    */
/*************************************************************/

/* 矩形 */
typedef struct irect {
    ipos pos;
    isize size;
}irect;
    
/* 矩形包含: iiok, iino */
int irectcontains(const irect *con, const irect *r);
/* 矩形包含: iiok, iino */
int irectcontainspoint(const irect *con, const ipos *p);
    
/*************************************************************/
/* icircle                                                   */
/*************************************************************/


/* 圆形 */
typedef struct icircle {
    ipos pos;
    ireal radius;
}icircle;

/*圆形的关系 */
typedef enum EnumCircleRelation {
    EnumCircleRelationBContainsA = -2,
    EnumCircleRelationAContainsB = -1,
    EnumCircleRelationNoIntersect = 0,
    EnumCircleRelationIntersect = 1,
} EnumCircleRelation;
    
/* 圆形相交: iiok, iino */
int icircleintersect(const icircle *con, const icircle *c);
/* 圆形包含: iiok, iino */
int icirclecontains(const icircle *con, const icircle *c);
/* 圆形包含: iiok, iino */
int icirclecontainspoint(const icircle *con, const ipos *p);
/* 圆形的关系: EnumCircleRelationBContainsA(c包含con), */
/*    EnumCircleRelationAContainsB(con包含c), */
/*    EnumCircleRelationIntersect(相交), */
/*    EnumCircleRelationNoIntersect(相离) */
int icirclerelation(const icircle *con, const icircle *c);
    
/* 矩形与圆是否相交 */
int irectintersect(const irect *con, const icircle *c);

/* 名字的最大长度 */
#define IMaxNameLength 32
    
/*************************************************************/
/* iname                                                     */
/*************************************************************/

/* 名字 */
typedef struct iname {
    char name[IMaxNameLength+1];
}iname;
   
/*************************************************************/
/* imeta                                                     */
/*************************************************************/

/* 内存操作 */
#define icalloc(n, size) calloc(n, size)
#define irealloc(ptr, size) realloc(ptr, size)
#define ifree(p) free(p)
    
#if iimeta  /* if iimeta */
    
/* 最多支持100个类型对象 */
#define IMaxMetaCountForUser 100
    
/* 前置声明 */
struct imeta;
    
/* 基础的内存对象, 都具备缓冲功能 */
typedef struct iobj {
    int size;
    struct imeta *meta;
    struct iobj *next;
    char addr[];
}iobj;
    
/* 基础内存对象缓存 */
typedef struct iobjcache {
    struct iobj *root;
    int length;
    int capacity;
}iobjcache;

/* 编码对象的元信息 */
typedef struct imeta {
    const char* name;
    struct iobjcache cache;
    int size;
    int64_t current;
    int64_t alloced;
    int64_t freed;
}imeta;
    
/* 获取类型的元信息 */
imeta *imetaget(int idx);
    
/* 也可以手动注册一个元信息来管理自己的对象: 然后就可以通过 iobjmalloc 进行对象内存管理 */
/* 将会返回对象的meta索引 */
int imetaregister(const char* name, int size, int capacity);
/* 获取索引 */
#define imetaindex(type) imeta_##type##_index
/* 注册宏 */
#define iregister(type, capacity) imetaregister(#type, sizeof(type), capacity)
/* 声明 */
#define irealdeclareregister(type) int imetaindex(type)
/* 声明 */
#define iideclareregister(type) extern irealdeclareregister(type)
/* 注册宏 */
#define irealimplementregister(type, capacity) imetaindex(type) = iregister(type, capacity)
/* 注册宏 */
#define iimplementregister(type, capacity) int irealimplementregister(type, capacity)
/* 获取meta */
#define imetaof(type) imetaget(imetaindex(type))


/* 定义所有内部对象的meta索引 */
#define __ideclaremeta(type, capacity) imetaindex(type)

#define __iallmeta                            \
    __ideclaremeta(iobj, 0),                  \
    __ideclaremeta(iref, 0),                  \
    __ideclaremeta(ireflist, 1000),           \
    __ideclaremeta(irefjoint, 200000),        \
    __ideclaremeta(inode, 4000),              \
    __ideclaremeta(iunit, 2000),              \
    __ideclaremeta(imap, 0),                  \
    __ideclaremeta(irefcache, 0),             \
    __ideclaremeta(ifilter, 2000),            \
    __ideclaremeta(isearchresult, 0),         \
    __ideclaremeta(irefautoreleasepool, 0),   \
    __ideclaremeta(iarray, 0),                \
    __ideclaremeta(islice, 0)

/* 定义所有元信息索引 */
typedef enum EnumMetaTypeIndex {
    __iallmeta,
   EnumMetaTypeIndex_imax,
}EnumMetaTypeIndex;

/* 获取响应的内存：会经过Meta的Cache */
void *iaoicalloc(imeta *meta);

/* 释放内存：会经过Meta的Cache */
void iaoifree(void *p);
    
/* 尽可能的释放Meta相关的Cache */
void iaoicacheclear(imeta *meta);

/* 打印当前内存状态 */
void iaoimemorystate();
    
/* 获取指定对象的meta信息 */
imeta *iaoigetmeta(const void *p);
    
/* 指定对象是响应的类型: 一致返回iiok, 否则返回 iino */
int iaoiistype(const void *p, const char* type);
    
#define imetacacheclear(type) (iaoicacheclear(imetaof(type)))

#define iobjmalloc(type) ((type*)iaoicalloc(imetaof(type)))
#define iobjfree(p) do { iaoifree(p); p = NULL; } while(0)
#define iobjistype(p, type) iaoiistype((void*)p, #type)
    
#else   /* #if iimeta */
    
/* 打印当前内存状态: 空的内存状态 */
void iaoimemorystate() ;
    
#define iobjmalloc(type) ((type*)icalloc(1, sizeof(type)))
#define iobjfree(p) do { ifree(p); p = NULL; } while(0)
#define iobjistype(p, type) iino
    
#endif  /* #if iimeta */

/*************************************************************/
/* iref                                                      */
/*************************************************************/

/* 定义引用计数，基础对象 */
#define irefdeclare volatile int ref; struct irefcache* cache; ientryfree free; ientrywatch watch
/* iref 转换成 target */
#define icast(type, v) ((type*)(v))
/* 转换成iref */
#define irefcast(v) icast(iref, v)

/* 前置声明 */
struct iref;
struct irefcache;

/* iref 的析构函数 */
typedef void (*ientryfree)(struct iref* ref);

/* iref 的跟踪函数: 引用计数减小的时候会通知 */
typedef void (*ientrywatch)(struct iref* ref);

/* 引用计数结构体 */
typedef struct iref {
    irefdeclare;
}iref;

/* 增加引用计数 */
int irefretain(iref *ref); 

/* 释放引用计数 */
void irefrelease(iref *ref);

/* 引用宏 */
#define iretain(p) do { if(p) irefretain((iref*)(p)); } while(0)

/* 释放宏 */
#define irelease(p) do { if(p) irefrelease((iref*)(p)); } while(0)
    
/* 应用计数的赋值操作 */
#define iassign(dst, src) do { if(src != dst) { irelease(dst); iretain(src); dst = src; } } while(0)

/*************************************************************/
/* irefautoreleasepool                                       */
/*************************************************************/

/* 前置声明 */
struct ireflist;

/* 自动释放池子 */
typedef struct irefautoreleasepool {
    struct ireflist *list;
}irefautoreleasepool;

/* 申请自动释放池子 */
irefautoreleasepool * irefautoreleasebegin();

/* 自动释放 */
iref* irefautorelease(irefautoreleasepool *pool, iref *ref);

/* 结束自动释放 */
void irefautoreleaseend(irefautoreleasepool *pool);

/* 返回值本身的，引用retain */
iref *irefassistretain(iref *ref);

/* 便利宏用来使用autoreleasepool */
#define _iautoreleasepool irefautoreleasepool* pool = irefautoreleasebegin()

#define _iautomalloc(type) ((type*)irefautorelease(pool, irefassistretain((iref*)iobjmalloc(type))))

#define _iautorelease(p) irefautorelease(pool, (iref*)p)

#define _iautoreleaseall irefautoreleaseend(pool)

/*************************************************************/
/* ireflist                                                  */
/*************************************************************/

/* 节点 */
typedef struct irefjoint {
    /* 附加的对象 */
    iref *value;
    
    /* 必要的校验 */
    struct ireflist *list;
    
    /* 列表节点 */
    struct irefjoint *next;
    struct irefjoint *pre;
}irefjoint;

/* 构造列表节点 */
irefjoint* irefjointmake(iref *value); 

/* 释放列表节点 */
void irefjointfree(irefjoint* joint); 

/* 营养对象列表 */
typedef struct ireflist {
    /* 列表根节点, 也是列表的第一个节点 */
    irefjoint *root;
    /* 列表长度 */
    int length;
    /* 时间 */
    int64_t tick;
}ireflist;

/* 创建列表 */
ireflist *ireflistmake();

/* 获取列表长度 */
int ireflistlen(const ireflist *list); 

/* 获取第一个节点 */
irefjoint* ireflistfirst(const ireflist *list); 

/* 从列表里面查找第一个满足要求的节点 */
irefjoint* ireflistfind(const ireflist *list, 
        const iref *value); 

/* 往列表增加节点: 前置节点 */
irefjoint* ireflistaddjoint(ireflist *list, irefjoint * joint); 

/* 往列表增加节点: 前置节点(会增加引用计数) */
irefjoint* ireflistadd(ireflist *list, iref *value); 

/* 从节点里面移除节点, 返回下一个节点 */
irefjoint* ireflistremovejoint(ireflist *list, irefjoint *joint); 
/* 从节点里面移除节点 , 并且释放当前节点, 并且返回下一个节点 */
irefjoint* ireflistremovejointandfree(ireflist *list, irefjoint *joint); 
/* 从节点里面移除节点: 并且会释放节点, 返回下一个节点 */
irefjoint* ireflistremove(ireflist *list, iref *value); 

/* 释放所有节点 */
void ireflistremoveall(ireflist *list); 

/* 释放列表 */
void ireflistfree(ireflist *list); 

/*************************************************************/
/* iarray                                                    */
/*************************************************************/
struct iarray;
struct islice;

/*如果是需要跟 arr_invalid 进行交换就是置0 */
#define arr_invalid -1

/* 交换两个对象 */
typedef void (*iarray_entry_swap)(struct iarray *arr,
        int i, int j);
/* 比较两个对象 */
typedef int (*iarray_entry_cmp)(struct iarray *arr,
        int i, int j);
/* 赋值 */
typedef void (*iarray_entry_assign)(struct iarray *arr,
        int i, void *value);

/* 数组常用控制项 */
typedef enum EnumArrayFlag {
    EnumArrayFlagNone = 0,

    /* 移除元素的时候，
     * 不移动数组，直接从后面替补
     * */
    EnumArrayFlagKeepOrder = 1<<1,  /*是否保持有序*/
    
    /*是否是简单数组, 
     *单元不需要通过swap或者assign去释放
     *truncate 的时候可以直接设置长度
     **/
    EnumArrayFlagSimple = 1<<2,
    
    /*自动缩减存储容量*/
    EnumArrayFlagAutoShirk = 1<<3,
}EnumArrayFlag;

/* 数组基础属性, 类型元信息 */
typedef struct iarrayentry{
    int flag;
    size_t size;
    iarray_entry_swap swap;
    iarray_entry_cmp cmp;
    iarray_entry_assign assign;
} iarrayentry;

/* 通用数组 */
typedef struct iarray {
    irefdeclare;

    size_t capacity;
    size_t len;
    char *buffer;

    /* 每一种数组类型都需要定义这个*/
    const iarrayentry* entry;
}iarray;

/* 建立数组*/
iarray *iarraymake(size_t capacity, const iarrayentry *entry);

/* 释放 */
void iarrayfree(iarray *arr);

/* 长度 */
size_t iarraylen(const iarray *arr);

/* 容量*/
size_t iarraycapacity(const iarray *arr);

/* 查询 */
void* iarrayat(iarray *arr, int index);

/* 数组内存缓冲区 */
void* iarraybuffer(iarray *arr);

/* 删除 */
int iarrayremove(iarray *arr, int index);

/* 增加 */
int iarrayadd(iarray *arr, void* value);

/* 清理数组 */
void iarrayremoveall(iarray *arr);

/* 截断数组 */
void iarraytruncate(iarray *arr, size_t len);

/* 缩减容量 */
size_t iarrayshrinkcapacity(iarray *arr, size_t capacity);

/* 排序 */
void iarraysort(iarray *arr);

/*************************************************************/
/* islice                                                    */
/*************************************************************/

typedef struct islice {
    irefdeclare;

    iarray *array;
    int begin;
    int end;
}islice;


/*************************************************************/
/* irefcache                                                 */
/*************************************************************/

/* 构造函数 */
typedef iref* (*icachenewentry)();

/* 缓存弃守接口: 缓冲区放不下了就会调用这个 */
typedef void (*icacheenvictedentry)(iref *ref);
   
/* Cache */
/* 从缓存里面拿的东西，是需要释放的 */
/* 缓存参与引用计数对象的管理 */
typedef struct irefcache{
    iname name;
    
    ireflist* cache;
    int capacity;
    
    icachenewentry newentry;
    icacheenvictedentry envicted;
}irefcache;

/* 创造一个cache */
irefcache *irefcachemake(int capacity, icachenewentry newentry); 

/* 从缓存里面取一个 */
iref *irefcachepoll(irefcache *cache); 

/* 释放到缓存里面: 只有 ref 真正没有被其他用到的时候才会回收到缓冲区重复使用 */
/* 可以用来代替 irelease 的调用 */
/* 即使不是通过irefcachepoll 获取的对象也可以放入缓存里面只要类型一致 */
void irefcachepush(irefcache *cache, iref *ref);
    
/* 当前缓冲区清理, 不能直接操作里面的list */
void irefcacheclear(irefcache *cache);
    
/* 释放缓存 */
void irefcachefree(irefcache *cache);
    
/* 当前缓冲区的存储的对象个数 */
int irefcachesize(irefcache *cache);

/* 用宏处理缓存接口: 拿 */
#define icache(cache, type) ((type*)irefcachepoll(cache))
/* 用宏处理缓存接口: 放回 */
#define icacheput(cache, ref) irefcachepush(cache, (iref*)(ref))

/* 最大的分割次数 */
#define IMaxDivide 32

/*************************************************************/
/* icode                                                     */
/*************************************************************/

/* 编码, 以0结尾，c 风格字符串 */
typedef struct icode {
    char code[IMaxDivide+1];
    ipos pos;
}icode;

/*************************************************************/
/* iuserdata                                                 */
/*************************************************************/

/* 自定义数据 */
typedef struct iuserdata {
    int u1, u2, u3, u4;
    void *up1, *up2, *up3, *up4;
}iuserdata;

/*************************************************************/
/* iunit                                                     */
/*************************************************************/

/* 前置声明 */
struct inode;

/* 基本单元状态 */
typedef enum EnumUnitState {
    EnumUnitStateNone = 0,
    EnumUnitStateDead = 1 << 3,
    EnumUnitStateFlying = 1<< 5,
    EnumUnitStateMoving = 1<< 6,
    EnumUnitStateSearching = 1<<10,
}EnumUnitState;

/* 单元 */
typedef struct iunit {
    irefdeclare;
    
    /* 名称 */
    iname name;
    /* ID */
    iid   id;
    
    /* 状态 */
    int64_t state;
    /* 更新时间(毫秒) */
    int64_t tick;
    
    /* 坐标 */
    ipos  pos;
    icode code;
    
#if iiradius
    /* 半径 */
    ireal radius;
#endif
    
    /* 用户数据 */
    iuserdata userdata;
    
    /* 在同一单元内部列表的关系 */
    struct iunit* next;
    struct iunit* pre;
    
    /* 所在的叶子节点 */
    struct inode* node;
}iunit;

/* 构造一个基本单元 */
iunit * imakeunit(iid id, ireal x, ireal y);
    
/* 重载：构造一个基本单元 */
iunit * imakeunitwithradius(iid id, ireal x, ireal y, ireal radius);

/* 释放一个基本单元 */
void ifreeunit(iunit *unit);

/* 释放链表 */
void ifreeunitlist(iunit *unit); 

/*************************************************************/
/* inode                                                     */
/*************************************************************/

/* 节点分割的次数 */
#define IMaxChilds 4

/* 分割次数 */
#define IDivide 2

/* 二维空间 */
#define IDimension 2

/* 节点的状态 */
typedef enum EnumNodeState {
    EnumNodeStateNone = 0,
    /* 查找标记，在搜索的时候会短暂的标记 */
    EnumNodeStateSearching = 1<<5,

    /* 寻路的时候代表不可走，需要避开 */
    EnumNodeStateNoWalkingAble = 1<<12,
    /* 寻路的时候代表整个可以走 */
    EnumNodeStateWalkingAble = 1<<13,

    /* 节点需要Hold 住，不能释放 */
    EnumNodeStateStatic = 1<<23,
    /* 节点可不可以附加单元 */
    EnumNodeStateNoUnit = 1<<24,
}EnumNodeState;

/* 是否追踪更新时间戳 */
#define open_node_utick (1)

/* 节点 */
typedef struct inode {
    /* 声明引用对象 */
    irefdeclare;
    /* 节点层级: 从1 开始（根节点为0） */
    int   level;
    /* 节点对应的起点坐标编码 code[level-1] */
    icode code;
    /* 相对父节点的索引 */
    int codei;
    /* 继承坐标系 */
    int x, y;
    
    /* 节点状态 */
    int64_t state;
    /* 管理时间戳(增加单元，减少单元，增加子节点，减少子节点会更新这个时间戳) */
    int64_t tick;
#if open_node_utick
    /* 更新时间戳(管理时间戳 + 单元移动时间戳) */
    int64_t utick;
#endif
    
    /* 四叉树 */
    struct inode *parent;
    struct inode *childs[IMaxChilds];
    
    /* 附加在上面的单元 */
    struct iunit *units;
    
    /* 记录节点状态 */
    int childcnt;
    int unitcnt;
    
    /* 所有叶子节点都串起来了: */
    /* 维护一个有序的叶子节点列表非常有用 */
    struct inode *pre;
    struct inode *next;
    
    /*
     * 构成了一个有向图，可在联通上做单向通行
     * */
    /* 所有可以到达当前节点的邻居 other ===> this */
    ireflist *neighbors;
    /* 可走的列表 this ===> other */
    ireflist *neighbors_walkable;
}inode;

/* 节点内存管理 */
inode * imakenode();
    
/* 从节点数里面移除 */
void ineighborsclean(inode *node);
    
/* 在有向图上加上一单向边 */
void ineighborsadd(inode *node, inode *to);
    
/* 在有向图上移除一条单向边 */
void ineighborsdel(inode *node, inode *to);

/* 释放节点本身 */
void ifreenode(inode *node);

/* 释放节点以及节点关联的单元 */
void ifreenodekeeper(inode *node); 

/* 释放节点组成的树 */
void ifreenodetree(inode *node);

/*************************************************************/
/* imap                                                     */
/*************************************************************/

/* 地图状态信息，统计数据 */
typedef struct imapstate {
    int64_t nodecount;
    int64_t leafcount;
    int64_t unitcount;
}imapstate;

/* 地图 */
typedef struct imap {
    /* 地图名称 */
    iname name;
    
    /* 地图基本信息，世界坐标系统的位置，以及大小 */
    ipos  pos;
    isize size;
    
#if iiradius
    /* 单位的最大半径 */
    ireal maxradius;
#endif
    
    /* 地图分割的层 */
    /* 512 的地图分割24 次以后的精度为：0.00003052 */
    int   divide;
    
    /* 地图编码库, 以及地图的精度信息 */
    char  code[IDimension][IDivide];
    isize nodesizes[IMaxDivide+1];
    /* 地图精度的平方: 用来做距离筛选 */
    ireal distances[IMaxDivide+1];
    
    /* 四叉树的根节点 */
    inode *root;
    
    /* 节点缓存 */
    irefcache *nodecache;
    
    /* 叶子节点 */
    inode *leaf;
    
    /* 地图状态信息 */
    imapstate state;
    
    /* 存储地图的原始阻挡的位图信息 bits-map*/
    char *blocks;
}imap;

/* 节点加入地图 */
int imapaddunitto(imap *map, inode *node, iunit *unit, int idx);

/* 从地图上移除 */
int imapremoveunitfrom(imap *map, inode *node, iunit *unit, int idx, inode *stop);

/* 根据坐标生成code */
int imapgencode(const imap *map, const ipos *pos, icode *code);

/* 计算Code */
/* y */
/* ^ */
/* | (B , D) */
/* | (A , C) */
/* -----------> x  */
/* y */
/* ^ */
/* | ((0, 1) , (1, 1)) */
/* | ((0, 0) , (1, 0)) */
/* -----------> x 
 */
/* 从编码生成坐标 */
int imapgenpos(imap *map, ipos *pos, const icode *code);
   
/* 编码移动方向 */
typedef enum EnumCodeMove {
    EnumCodeMoveLeft,
    EnumCodeMoveRight,
    EnumCodeMoveDown,
    EnumCodeMoveUp,
    EnumCodeMoveMax
}EnumCodeMove;
/* 移动编码: 失败返回0, 成功返回移动的步骤数 */
int imapmovecode(const imap *map, icode *code, int way);

/* 建议 divide 不要大于 10*/
/* 生成一张地图数据 */
imap *imapmake(const ipos *pos, const isize *size, int divide);

/* 地图信息类型 */
typedef enum EenumMapState {
    EnumMapStateHead = 1,
    EnumMapStateTail = 1<<1,
    EnumMapStateBasic = 1<<2,
    EnumMapStatePrecisions = 1<<3,
    EnumMapStateNode = 1<<4,
    EnumMapStateUnit = 1<<5,
    
    /* 所有信息 */
    EnumMapStateAll = 0xffffffff,
    /* 除了头部和尾部 */
    EnumMapStateAllNoHeadTail = 0xffffffff
    & ~EnumMapStateHead
    & ~EnumMapStateTail,
    /* 不打印信息 */
    EnumMapStateNone = 0
}EenumMapState;

/* 打印地图状态信息 */
void imapstatedesc(const imap *map, int require,
                   const char* intag, const char *inhead);

/* 释放地图数据，释放附加在地图上的单元数据 */
void imapfree(imap *map); 

/* 生成地图数据 */
int imapgen(imap *map);

/* 增加一个单元到地图上 */
int imapaddunit(imap *map, iunit *unit); 

/* 从地图上移除一个单元 */
int imapremoveunit(imap *map, iunit *unit);

/* 从地图上检索节点 */
inode *imapgetnode(const imap *map, const icode *code, 
        int level, int find); 

/* 更新一个单元在地图上的数据 */
int imapupdateunit(imap *map, iunit *unit);
    
/* 更新一个单元的附加信息到地图数据上：现阶段就只更新了单元的半径信息 */
/* 如果单元改变了半径，需要调用这个函数刷新一下，才回立刻生效，不然等单位移动单元格后才生效*/
void imaprefreshunit(imap *map, const iunit *unit);
    
/* 设置块的状态 */
void imapsetblock(imap *map, int x, int y, int state);

/* 加载位图阻挡信息 sizeof(blocks) == (divide*divide + 7 ) / 8 */
void imaploadblocks(imap *map, char* blocks);

/* 获取块的状态 */
int imapgetblock(const imap *map, int x, int y);
    
/*************************************************************/
/* ifilter                                                   */
/*************************************************************/

/* 前置声明 */
struct ifilter;
    
/* 过滤器行为 */
typedef enum EnumFilterBehavior {
    EnumFilterBehaviorChooseRangeNode = 1,
}EnumFilterBehavior;

/* 过滤器入口函数 */
typedef int (*ientryfilter)(imap *map, const struct ifilter *filter, const iunit* unit);

/* 过滤器指纹入口 */
typedef int64_t (*ientryfilterchecksum)(imap *map, const struct ifilter *filter);

/* 过滤器上下文 */
typedef struct ifilter {
    irefdeclare;
    /* 过滤器上下文 */
    struct {
        /* 基础属性 */
        union {
            icircle circle;
            irect rect;
            icode code;
            int64_t id;
        }u;
        /* 复合过滤器 */
        ireflist *list;
    }s;
    
    /* 过滤器入口 */
    ientryfilter entry;
    /* 指纹 */
    ientryfilterchecksum entrychecksum;
    /* 选择合适的子节点 */
    int behavior;
}ifilter;

/* 指纹识别 */
int64_t ifilterchecksum(imap *map, const ifilter *d);

/* 释放节点o */
void ifilterfree(ifilter *filter); 

/* 基础过滤器 */
ifilter *ifiltermake(); 

/* 往一个已有的过滤器里面添加过滤器 */
void ifilteradd(ifilter *filter, ifilter *added); 

/* 移除子过滤器 */
void ifilterremove(ifilter *filter, ifilter *sub);
    
/* 移除所有子过滤器 */
void ifilterclean(ifilter *filter);

/* 通用过滤器入口 */
int ifilterrun(imap *map, const ifilter *filter, const iunit *unit); 

/* circle 过滤器 */
ifilter *ifiltermake_circle(const ipos *pos, ireal range);
    
/* rect 过滤器 */
ifilter *ifiltermake_rect(const ipos *pos, const isize *size);

/* 搜集树上的所有单元, 调用完后必须调用imapcollectcleanunittag */
void imapcollectunit(imap *map, const inode *node, ireflist *list, const ifilter *filter, ireflist *snap);
/* 清除搜索结果标记 */
void imapcollectcleanunittag(imap *map, const ireflist *list);
/* 清除搜索结果标记 */
void imapcollectcleannodetag(imap *map, const ireflist *list);

/*************************************************************/
/* isearchresult                                             */
/*************************************************************/

/* 搜索结果 */
typedef struct isearchresult {
    /* 单元 */
    ireflist* units;
    /* 过滤器 */
    ifilter * filter;
    
    /* 时间点 */
    int64_t tick;
    /* 上下文校验 */
    int64_t checksum;
    /* 快照 */
    ireflist* snap;
}isearchresult;

/* 创建搜索结果 */
isearchresult* isearchresultmake(); 

/* 对接过滤器 */
void isearchresultattach(isearchresult* result, ifilter *filter);

/* 移除过滤器 */
void isearchresultdettach(isearchresult *result); 

/* 释放所有节点 */
void isearchresultfree(isearchresult *result);
    
/* 清理搜索的内容，后续的搜索会从新开始搜索 */
void isearchresultclean(isearchresult *result);
    
/* 从快照里面从新生成新的结果 */
void isearchresultrefreshfromsnap(imap *map, isearchresult *result);
    
/* 收集包含指定矩形局域的节点(最多4个) */
void imapsearchcollectnode(imap *map, const irect *rect, ireflist *list);
    
/* 计算给定节点列表里面节点的最小公共父节点 */
inode *imapcaculatesameparent(imap *map, const ireflist *collects);
    
/* 从地图上搜寻单元 irect{pos, size{rangew, rangeh}}, 并附加条件 filter */
void imapsearchfromrectwithfilter(imap *map, const irect *rect,
                                  isearchresult *result, ifilter *filter);
    
/* 从地图上搜寻单元 */
void imapsearchfrompos(imap *map, const ipos *pos,
                       isearchresult *result, ireal range);
    
/* 从地图上搜寻单元, 不包括自己 */
void imapsearchfromunit(imap *map, iunit *unit,
                        isearchresult *result, ireal range);
    
/* 搜索: 最后的搜索都会经过这里 */
void imapsearchfromnode(imap *map, const inode *node,
                        isearchresult* result, ireflist *innodes);
    
/* 计算节点列表的指纹信息 */
int64_t imapchecksumnodelist(imap *map, const ireflist *list, int64_t *maxtick, int64_t *maxutick);

/*************************************************************/
/* print helper                                              */
/*************************************************************/

/* 打印树的时候携带的信息 */
typedef enum EnumNodePrintState {
    EnumNodePrintStateTick = 1,
    EnumNodePrintStateUnits = 1 << 1,
    EnumNodePrintStateMap = 1 << 2,
    EnumNodePrintStateNode = EnumNodePrintStateTick | EnumNodePrintStateUnits,
    EnumNodePrintStateAll = 0xFFFFFFFF,
}EnumNodePrintState;
    
/* 打印地图 */
void _aoi_print(imap *map, int require);
/* 打印指定的节点*/
void _aoi_printnode(int require, const inode *node, const char* prefix, int tail);
    
/* 测试 */
int _aoi_test(int argc, char** argv);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* __AOI_H_ */


