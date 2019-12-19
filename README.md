## 实时系统hw2，修改freertos内核以实现EDF等调度 ##
项目主体位于FreeRTOS/WIN32-MSVC中，用visual studio打开。
  这个项目文件夹是从FreeRTOS/Demo/WIN32-MSVC复制而来，在此基础上修改。
  新的edf任务函数是xTaskDeadlineCreate，在原有基础上加了一个DDL的参数。

2019.12.18
根据某论文和其它git repo改写了EDF算法：
在task的代码也就是workload相同时通过改deadline可以看到EDF的效果。
但是workload变更之后会变成smallest workload first，这大概和编译时间的长短有关，还是说原本freertos的策略？
一个方法是创建一个执行时间长的workload，保证其他task在它后面排队，另外就是进一步看看freertos的内核。
