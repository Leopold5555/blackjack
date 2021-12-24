# blackjack MySQL plugin project

Пример создания UDF плагина для MySQL

## Плагин позволяет:

#### 1. **Выполнять HTTP/HTTPS запросы прямо из MySQL.**

Пример HTTP запроса:

```
mysql> SELECT wget("https://www.xxxxxxx.com/black/?valute=USD");

+-----------------------------------------------------+
| wget("https://www.xxxxxxx.com/black/?valute=USD")   |
+-----------------------------------------------------+
| {"valute":"USD","course":17.8285,"date":"20211223"} |
+-----------------------------------------------------+
1 row in set (0.84 sec)
```



#### 2. **Организовать защиту mysql базы данных при помощи лицензионных ключей.** 

Пример проверки лицензии:

```
mysql> select blackjack("HBLEHB-DGIKMP-PLMAMF-IAKN", 555555 );
+-------------------------------------------------+
| blackjack("HBLEHB-DGIKMP-PLMAMF-IAKN", 555555 ) |
+-------------------------------------------------+
| VALID                                           |
+-------------------------------------------------+
1 row in set (0.00 sec)

mysql> select blackjack("HBLEHB-DEIKMP-PLMAMF-AEKO", 555555 );
+-------------------------------------------------+
| blackjack("HBLEHB-DEIKMP-PLMAMF-AEKO", 555555 ) |
+-------------------------------------------------+
| EXPIRED                                         |
+-------------------------------------------------+
1 row in set (0.00 sec)
```

Полнофункциональный пример смотри в файле `black.sql`  

На примере таблицы products выполнена защита от добавления новых данных после завершения действия лицензионного ключа.

```
mysql> INSERT INTO `products` ( `product_name`, `product_price`)  VALUES ( "Beer baltika 9", 72.3 );
    
ERROR 1644 (45000): CHECK LICENSE
```


В папке `keygen` находится генератор лицензионных ключей. Для сборки генератора достаточно запустить скрипт `build_keygen`

В папке `plugn` находится реализация плагина. Его сборка осуществляется посредством частичной сборки исходников `MySQL Server 5.7`. Процесс сборки автоматизирован скриптом `build_plugin`

**ВАЖНО!** Для сборки плагина, необходимо, сперва, установить библиотеку `boost` желательно версии `1.59.0`. Лучше это делать путем компиляции из исходников [https://www.boost.org/users/history/version_1_59_0.html]

После компиляции, файл `udf_black.so` необходимо скопировать в папку плагинов MySQL (обычно это `/usr/lib64/mysql/plugin`)

Далее регистрируем плагин в `MySQL`:

```
CREATE FUNCTION blackjack RETURNS STRING SONAME "udf_black.so"; 
CREATE FUNCTION wget RETURNS STRING SONAME "udf_black.so";

```

Если возникнут проблемы с компиляцией - в папке `keygen/bin/` есть уже откомпилированный бинарник генератора ключей. А в папке `plugin/bin_for_MySQL_5.7.36` есть готовый, откомпилированный плагин, который теоретически подойдет к любому `MySQL` серверу версии 5.7
 
### (c) 2021 Leonid B. TI-182 f/r
