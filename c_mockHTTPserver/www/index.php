<?php
    echo "hello world!"."<br/>";
    echo "id = ".$_GET["id"]."<br/>";       // $代表php中的变量，_GET取URL变量名的对应值
    echo "name =".$_GET["name"]."<br/>"; 
    phpinfo();  // 打印php参数
?>