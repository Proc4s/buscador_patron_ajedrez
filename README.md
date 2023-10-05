# buscador_patron_ajedrez
Guia telegrafica.

Código utilizado en el TFM Patrones de Ajedrez.

Probado en Ubuntu 22.04 LTS.

Paquetes que es necesario tener instalados:

  gcc
  
  binutils
  
  openjdk
  
  libc6_dev
  
  SQLite
  
  Apache hadoop
  
  Apache cassandra
  
  python 3
  
    pysimplegui
    
    ipcqueue
    
    datetime
    
    os
    
    sys
    
    tkinter
    
    time

  Compilación fuentes en src.
  
    - crear carpeta bin
    
    - cd src
    
    - make

  Compilación del resto: ir a la carpeta correspondiente y ejecutar "make".

  La aplicación de pruebas/java-genbaselimpia esta desarrollada con "visual studio code" de microssof.

  Para ejecutar las pruebas se ha creado el usuario "hadoop" y se ha instalado hadoop en el, así como python y el resto de paquetes de este con "anaconda".
  
  Se ha creado el árbol:
  
    /home/hadoop/ajedrez
    
      base
      
        base_0 => link simbolico a carpeta que contiene base sqlite en disco 0.
        
        base_1 => link simbolico a carpeta que contiene base sqlite en disco 1.
        
      bin
      
        todos los ejecutables resultado de la compilación de src más "control.py"
        
      conf
      
        base.conf
        
        job.conf
        
      data
      
        salida (carpeta)
        
        patronRegaloxxx.bin
        

  en .bashrc añadir: export PATHAJEDREZ=/home/hadoop/ajedrez
  
  en hadoop/etc/hadoop modificar: mapred-site.xml con el siguiente contenido:
  
      <configuration>
        <property>
            <name>mapreduce.framework.name</name>
            <value>yarn</value>
        </property>
        <property>
            <name>mapreduce.task.timeout</name>
            <value>4000000</value>
        </property>
        <property>
            <name>PATHAJEDREZ</name>
            <value>/home/hadoop/ajedrez</value>
        </property>
        <property>
            <name>mapreduce.job.maps</name>
            <value>4</value>
        </property>
    </configuration>
    
La utilidad de los distintos programas es la siguiente:

  control.py  => programa en python de control del buscador.
  
  creabaseSqlite => programa para crear las bases sqlite junto con las tablas necesarias.
  
  fich2sqlite => programa para cargar las bases sqlite a partir de un fichero indexado.
  
  genbasfich => carga de un fichero indexado desde ficheros en formato PGN
  
  gpatronbin => compilador de patrón, a partir de un texto genera patron.bin
  
  mapbpatronsql => programa buscador de patrones (mapper de hadoop)
  
  sellistapart => programa de consulta a tabla de particiones de sqlite para obtener lista de particiones a procesar.
  

Para ejercitar el buscador, supuesto que están cargadas las bases sqlite y puesto en marcha HADOOP
  
      ejecutar bin/control.py
        
  
