# shell

## Búsqueda en $PATH

**¿Cuáles son las diferencias entre la syscall execve(2) y la familia de wrappers proporcionados por la librería estándar de C (libc) exec(3)?¿Puede la llamada a exec(3) fallar? ¿Cómo se comporta la implementación de la shell en ese caso?**

En primer lugar, execve(2) es una llamada al sistema que recibe tres argumentos:

- La ruta completa del archivo ejecutable.
- Un vector de argumentos para el nuevo programa.
- Un vector de variables de entorno para el nuevo programa.
  
Su función es ejecutar el programa recibido, reemplazando el proceso actual con el nuevo programa.

En cambio, la familia de funciones exec(3) se basa en execve(2) y otorga una interfaz más fácil de usar para los desarrolladores, encargándose este también de reemplazar el proceso actual por uno nuevo.
Esta función recibe diferentes argumentos según la variante utilizada.

Por lo tanto, la elección entre ellas depende de las necesidades específicas. Si lo que se busca es tener el control total sobre la ruta del programa, definir variables de entorno precisas o tener más flexibilidad para personalizar la ejecución del nuevo programa, entonces es más conveniente utilizar la primera opción.

En nuestra implementación de la shell, nosotros utilizamos execvp(3), que es una función de la familia exec(3) utilizada para buscar el programa en el path y proporcionar argumentos como un vector, y esta llamada puede fallar cuando el programa no es encontrado, hay errores en el archivo ejecutable o permisos insuficientes.
Para manejar este fallo, finalizamos la ejecución de manera abrupta y liberamos la memoria del comando.

## Procesos en segundo plano

**Detallar cuál es el mecanismo utilizado para implementar procesos en segundo plano.**

El mecanismo que utilizamos nosotros para implementar estos procesos en segundo plano, sin tener en cuenta la implementacion de proceso en segundo plano avanzado, es utilizar un wait no bloqueante que verifique si hubo un proceso hijo en segundo plano que haya terminado, y en ese caso imprimir su background info.

Ya manejando en segundo plano avanzado, el objetivo es inicializar un stack alternativo para definir dentro de el la funcion handler y asi, en caso de que un proceso termine, capturar la signal SIGCHLD y, filtrando el process group id para determinar que es un proceso en segundo plano, ejecutar la funcion handler e imprimir la informacion al finalizar su ejecucion.

## Flujo estándar

**Investigar el significado de 2>&1, explicar cómo funciona su forma general**

**Mostrar qué sucede con la salida de cat out.txt en el ejemplo.
Luego repetirlo, invirtiendo el orden de las redirecciones (es decir, 2>&1 >out.txt). ¿Cambió algo? Compararlo con el comportamiento en bash(1).**

Respuesta:
La forma general de la expresión dada es "X>&Y", siendo "X" el descriptor de archivo que se quiere redirigir, "Y" el descriptor de archivo destino y ">&" el operador para redireccionar una salida a otro descriptor.
Por lo tanto, "2>&1" significa que se redirige el descriptor del archivo 2 (la salida de error estándar, stderr) al descriptor de archivo 1 (la salida estándar, stdout).

En el ejemplo, al ejecutar:

      ls -C /home /no existe >out.txt 2>&1
      cat out.txt

Obtenemos como salida:

      ls: cannot access '/no existe': No such file or directory
      /home:
      ubuntu  vagrant

Lo que sucede acá es que como el archivo que se quiere listar no existe, se genera un mensaje de error, y tanto la salida STDOUT como la salida STDERR se redirigen al archivo out.txt (creándose uno nuevo si no existe). Por lo tanto, al hacer "cat out.txt" se ven los dos resultados: la salida de "ls -C /home" y el mensaje de error al listar "/noexiste".
En nuestra implementación, no ocurre un cambio al invertir el orden de las redirecciones y se comporta igual que la bash(1) en el primer caso (>out.txt 2>&1).
Sin embargo, al ejecutar el invertido (2>&1 >out.txt), observamos que se imprime el mensaje de error y este no se redirecciona al archivo out.txt, conteniendo únicamente la salida generada por "ls". Esto se debe a que al ejecutarse primero "2>&1" se redirige STDERR al mismo destino que STDOUT, apuntando ambos a la terminal que es donde STDOUT apunta originalmente. Finalmente, STDOUT se redirige al out.txt, pero como la redirección de STDERR se realizó en el paso anterior, STDERR sigue apuntando a la terminal.

## Tuberías múltiples

**Investigar qué ocurre con el exit code reportado por la shell si se ejecuta un pipe**

- **¿Cambia en algo?**
- **¿Qué ocurre si, en un pipe, alguno de los comandos falla? Mostrar evidencia (e.g. salidas de terminal) de este comportamiento usando bash. Comparar con su implementación.**

Al ejecutarse un pipe, si todos los comandos en el pipe se ejecutan de manera exitosa (con un código de salida 0), la shell reporta un exit code global igual a 0. En cambio, si uno de los comandos en el pipe falla (con un código de salida diferente a 0), el exit code global es el código de salida del último comando que falló en el pipe.

bash:

      $ ls -l ~/ | grep Doc
      drwxr-xr-x 4 4096 abr 10 16:02 Documents
      $ ls -l ~/no_existe | grep Doc
      ls: cannot access '/home/no_existe': No such file or directory
      $ echo "${PIPESTATUS[0]} ${PIPESTATUS[1]}"
      2 1

Nuestra implementación:

      $ ls -l | grep D
      drwxr-xr-x 2  4096 abr 10 15:34 Desktop
      drwxr-xr-x 4  4096 abr 10 16:02 Documents
      drwxr-xr-x 3  4096 abr 21 14:26 Downloads
         Program terminated: [676516] with status: 0
      $ ls -l no_existe | grep D
      ls: cannot access 'no_existe': No such file or directory
         Program terminated: [676651] with status: 256

Como vemos, a diferencia de nuestra implementación, bash maneja el estado de los pipes por separado.

## Variables de entorno temporarias

**¿Por qué es necesario hacerlo luego de la llamada a fork(2)?**

La llamada a las variables de entorno temporales se realiza después del fork porque solo viven durante la ejecución del proceso que se encarga de ejecutar el comando. No es necesario que sobrevivan a toda la ejecución de la shell debido a que son temporales.

**En algunos de los wrappers de la familia de funciones de exec(3) (las que finalizan con la letra e), se les puede pasar un tercer argumento (o una lista de argumentos dependiendo del caso), con nuevas variables de entorno para la ejecución de ese proceso. Supongamos, entonces, que en vez de utilizar setenv(3) por cada una de las variables, se guardan en un arreglo y se lo coloca en el tercer argumento de una de las funciones de exec(3).**

- **¿El comportamiento resultante es el mismo que en el primer caso? Explicar qué sucede y por qué?**
- **Describir brevemente (sin implementar) una posible implementación para que el comportamiento sea el mismo.**

En el primer caso, cuando se utiliza setenv para establecer variables de entorno, cada variable se configura individualmente y cada llamada a setenv agrega o modifica una variable de entorno específica. Por lo que, el comportamiento resultante es que las variables de entorno se establecen de forma independiente y se mantienen durante la ejecución de un proceso hijo que pueda existir.

En el segundo caso, en lugar de usar setenv para cada variable, se puede almacenar todas las variables de entorno en un arreglo y pasar ese arreglo como tercer argumento a una función exec. Sin embargo, realizandolo de esta forma las variables de entorno se establecen todas juntas como un conjunto, lo cual puede generar unos comportamientos diferentes ya que unicamente se estarian creando nuevas variables de entorno o, en caso de que ya haya definida una variable de entorno, se sobreescribira con el contenido del array; mientras que si no existia previamente, se creara con el valor del arreglo. Esto tambien puede generar que, si alguna variable de entorno no esta en el arreglo, se eliminara de un proceso hijo.

Para que el comportamiento sea el mismo que al usar setenv, existe la posibilidad de implementar una función que tome un arreglo de variables de entorno y configure cada una de ellas utilizando setenv de manera individual, iterando sobre el arreglo y llamando aquella syscall para cada variable, lo cual aseguraria que las variables se establezcan de forma independiente y se mantengan durante la ejecucion del proceso hijo.

## Pseudo-variables

**Investigar al menos otras tres variables mágicas estándar, y describir su propósito. Incluir un ejemplo de su uso en bash (u otra terminal similar).**

- **$_**: Al inicio de la shell, se establece con la ruta del archivo que invoco la shell o el script actual. Ademas, despues de la ejecución de un comando simple en primer plano, se expande al ultimo argumento del comando previo.
Ejemplo de uso:

      $ echo 1 2
      1 2
      $ echo $_
      2

- **$!** Contiene el ID del proceso del ultimo comando ejecutado en segundo plano (background). Es util para rastrear procesos en segundo plano.
Ejemplo de uso:

      $ sleep 10 &
      [1] 52638
      $ echo $!
      52638


- **$@** Expande a todos los argumentos pasados a un script o función como una lista separada por espacios, lo cual es util para iterar sobre los argumentos.
Ejemplo de uso:

      for arg in "$@"; do
            echo "Argumento: $arg"
      done

Cuya salida, ejecutando este comando con arg1, arg2 y arg3 como argumentos, seria:

      Argumento: arg1
      Argumento: arg2
      Argumento: arg3


## Comandos built-in

**¿Entre cd y pwd, alguno de los dos se podría implementar sin necesidad de ser built-in? ¿Por qué? ¿Si la respuesta es sí, cuál es el motivo, entonces, de hacerlo como built-in? (para esta última pregunta pensar en los built-in como true y false)**

Entre cd y pwd, el comando que puede ser implementado sin necesidad de ser built-in es el comando pwd. Esto se debe a que cd es sumamente necesario implementarse en el mismo proceso que ejecuta la shell debido a que se produce el cambio de directorio, cosa que debe persistir para los siguientes prompts. Por otro lado, el comando pwd no afecta el funcionamiento del proceso general debido a que unicamente imprime el directorio, y por eso este comando puede implementarse sin la necesidad de ser built-in.

En nuestra implementacion, el motivo por el cual lo implementamos built-in es para simplemente imprimir el directorio actual, y no ejecutarlo como un comando con exec, por lo que simplifica bastante su tarea.

## Segundo plano avanzado

**¿Por qué es necesario el uso de señales?**

El uso de señales es sumamente necesario para implementar los procesos en segundo plano avanzado debido a que estos procesos secundarios emiten señales las cuales podemos atrapar en base a nuestra preferencia y asi manejarlas asignando un handler.

El handler es una funcion la cual la seteamos con la syscall sigaction para ejecutar cuando se emite una señal en especifico. De esta forma, manejando la syscall SIGCHLD la cual es emitida cuando un proceso termina, de esta forma (filtrando en base al process group id) se puede ejecutar la funcion handler cuando se termina un proceso en segundo plano con un process group id especifico.

## Historial

---
