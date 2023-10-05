#!/usr/bin/python
# Programa de control del sistema de busqueda de patrones.
#
# Autor.: Antonio Pardo Redondo
#
# Interfaz de usuario, solicita parametros de busca y lanza esta
# ejecutando HADOOP. Muestra el progreso de la operacion y permite abortar esta.
#
import PySimpleGUI as sg
import datetime
import os
import sys
import tkinter as tk
import subprocess
from ipcqueue import posixmq
from ipcqueue.serializers import RawSerializer
from time import sleep

# obtencion variables de entorno $PATHAJEDREZ y $HADOOP_HOME
PATHAJEDREZ = os.environ.get('PATHAJEDREZ')
HADOOP_HOME = os.environ.get('HADOOP_HOME')

# Valores de algunas variables de configuracion
BASENAME = ''
BASENUM = ''
FIFO = ''

# obtencion nombre base de datos: BASENAME
resultado = subprocess.run('grep BASENAME ' + PATHAJEDREZ + '/conf/base.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if resultado.returncode == 0:
	print(resultado.stdout)
	aux =[item.strip() for item in resultado.stdout.split('=')]
	BASENAME = aux[1]
else:
	print('Error al obtener el nombre de la base.')
	
# obtencion el numero de bases de datos: BASENUM
resultado = subprocess.run('grep BASENUM ' + PATHAJEDREZ + '/conf/base.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if resultado.returncode == 0:
	print(resultado.stdout)
	aux =[item.strip() for item in resultado.stdout.split('=')]
	BASENUM = aux[1]
else:
	print('Error al obtener el numero de la base.')
	
# obtencion nombre cola comunicaciones: FIFO
resultado = subprocess.run('grep FIFO ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if resultado.returncode == 0:
	aux =[item.strip() for item in resultado.stdout.split('=')]
	FIFO = aux[1]
else:
	print('Error al obtener el numero de la base.')

# Totalizadores globales.
particiones = 0
partidas = 0
hallados = 0
empieza = 0
ejecucion = False
vueltas = 0
nlineas = 1

# Valores de campos.
fileidmin = 0
fileidmax = 0
salida = ''

# Creacion-apertura cola de mensajes.
q1 = posixmq.Queue(FIFO,serializer=RawSerializer)
os.chmod('/dev/mqueue'+FIFO,0o666)

# Interfaz grafica.
# ================
# opciones globales.
sg.theme('LightGrey1')   # Add a touch of color
sg.set_options(font=('Arial', 14))

# Crear una instancia de Tkinter para obtener información sobre la pantalla
root = tk.Tk()
root.withdraw()  # Ocultar la ventana principal de Tkinter

# Obtener las dimensiones del monitor actual
screen_width = 300
screen_height = root.winfo_screenheight()/2

# Elementos de la interfaz.
l1 = sg.Text('Fecha Ini.:')													# rotulo Fecha inicial
l2 = sg.InputText(key="dateini", enable_events=True,size=(8,1))	# input Fecha inicial
l3 = sg.CalendarButton("Calendario",close_when_date_chosen=False,location=(screen_width/2+800, screen_height/2+25), format = '%Y/%m')			# calendario para fecha inicial
l4 = sg.Text('Fecha Fin.:')													# rotulo Fecha final
l5 = sg.InputText(key="datefin", enable_events=True,size=(8,1))	# input Fecha final
l6 = sg.CalendarButton("Calendario",close_when_date_chosen=False,location=(screen_width/2+800, screen_height/2+25), format = '%Y/%m')			# calendario para fecha final
l7 = sg.Text('EloMin.:')														# rotulo ELO minimo
l8 = sg.InputText(key="elomin",size=(6,1))								# input ELO minimo
l9 = sg.Text('EloMax.:')														# rotulo ELO max.
l10 = sg.InputText(key="elomax",size=(6,1))								# input ELO max
l11 = sg.Checkbox("Ganan Blancas",key="blancas")						# input ganan blancas
l12 = sg.Checkbox("Ganan Negras",key="negras")							# input ganan negras
l13 = sg.Text('Patrón.txt.:')													# rotulo Path del fichero patron 
l14 = sg.InputText(key="patron")												# input Path fichero patron
l15 = sg.FileBrowse()															# navegador Path fichero patron
l16 = sg.Radio("FEN","fsal",key='fen',default=True)					# Formato de salida FEN
l17 = sg.Radio("IMG","fsal",key='img')										# Formato de salida IMAGEN
l18 = sg.Button(key="Start",button_text="Start",button_color="green")	# Boton comienzo de tarea
l19 = sg.Text('',key='fase',enable_events=True)							# Indicacion de fase de la tarea
l20 = sg.ProgressBar(100,orientation='h',expand_x=True,size=(20,20),key='pbar')				# Barra progreso de la tarea
l21 = sg.Text('',key='pbarval',justification='center', enable_events=True, expand_x=True)	# procentaje de progreso tarea
l22 = sg.Text("Encontrados.:")												# rotulo Casos de verificacion Patron encontrados
l23 = sg.Text('',key='encontrados',enable_events=True)				# indicador de patron encontrados.
l24 = sg.Text('Fich. Sal.: ')													# rotulo Path del fichero patron 
l25 = sg.InputText(key="salida")												# input Path fichero patron
l26 = sg.FileSaveAs()

# Layout y agrupamientos parciales.

#columnas campos selector de la base.
col1 = sg.Column([[l1],[l4]])
col2 = sg.Column([[l2,l3],[l5,l6]])
col3 = sg.Column([[l7],[l9]])
col4 = sg.Column([[l8],[l10]])
fr0 = sg.Frame("Selector Base",[[col1,col2,col3,col4],[l11,l12]])

#selector formato de salida.
col5 = sg.Column([[l16],[l17]])
fr1 = sg.Frame("Formato Sal",[[col5]],expand_y=True,expand_x=True)

#selector de fichero patron
fr2 = sg.Frame("Fich. Patron",[[l13,l14,l15]],expand_x=True)

#selector de fichero de salida
fr3 = sg.Frame("Fich. Salida",[[l24,l25,l26]])

#zona de ejecucion.
fr4 = sg.Frame("Ejecución",[[l18,l19],[l20],[l21],[l22,l23]],expand_x=True)

# Layout general.
layout = [[fr0,fr1],[fr2],[fr3],[fr4]]

# Create the Window
window = sg.Window('Chess Job Control', layout, location=(screen_width//2, screen_height//2))
event, values = window.read(timeout = 40)

# funcion para comprobar si hadoop esta rodando.	
def is_hadoop_running():
	# Ejecutar el comando 'jps' y capturar su salida
	resultado = subprocess.run('jps | grep NameNode', shell=True, text=True)
	if resultado.returncode == 0:
		return True
	else :
		return False
		
# funcion de start HADOOP si no esta ejecutandose
def start_hadoop():
	if not is_hadoop_running():	
		print("Esperando a que Hadoop se inicie...")
		window["fase"].update("Esperando a que Hadoop se inicie...")
		event, values = window.read(timeout = 40)
		resultado = subprocess.run(['start-all.sh'], check=True)	
		if resultado.returncode == 0:
			print("Hadoop se ha iniciado correctamente.")
			window["fase"].update("Hadoop iniciado...")
			event, values = window.read(timeout = 40)
			return True
		else :
			print("Error al iniciar Hadoop.")
			return False
	else:
		print("Hadoop ya está en ejecución.")
		return True


# Función para validar el formato de fecha
def es_fecha_valida(fecha_texto):
    try:
        datetime.datetime.strptime(fecha_texto, '%Y/%m')
        return True
    except ValueError:
        return False

# funcion para validar los valores actuales de los campos.
# determinar sus valores y escribir fichero conf/job.conf.
def validar(values):
	global fileidmin
	global fileidmax
	global salida
	elomin = 0
	elomax = 0
	dateini = datetime.datetime.now()
	datefin = datetime.datetime.now()
	fileidmin = 0
	fileidmax = 0
	patron = ''
	salida = ''
	ganador = 0
	formasal = 0
	
	window["fase"].update("Validando Campos..")
	event, values = window.read(timeout = 40)
	# Fecha inicial
	if not es_fecha_valida(values['dateini']):
		return 'Se tiene que especificar una fecha inicial'
	else:
		dateini = values['dateini']
	# Fecha final
	if not es_fecha_valida(values['datefin']):
		return 'Se tiene que especificar una fecha final.'
	else:
		datefin = values['datefin']
	# Fecha inicial > fecha final
	if datetime.datetime.strptime(values['datefin'], '%Y/%m') < datetime.datetime.strptime(values['dateini'], '%Y/%m'):
		return 'La fecha final seleccionada tiene que ser posterior o igual a la fecha inicial.'
	# ELO min
	if not values['elomin'].isdigit():
		return 'Se tiene que especificar un número entero como elo minimo.'
	# ELO min
	if int(values['elomin']) < 0:
		return 'Se tiene que especificar un elo minimo.'
	else:
		elomin = values['elomin']
	# ELO max
	if not values['elomax'].isdigit():
		return 'Se tiene que especificar un número entero como elo maximo.'	
	# ELO max
	if int(values['elomax']) < 0:
		return'Se tiene que especificar un elo maximo.'
	else:
		elomax = values['elomax']
	# ELO min > ELO max
	if int(values['elomax']) < int(values['elomin']):
		return'Se tiene que especificar un elo maximo superior o igual al elo minimo.'
	# Patron 
	if not values['patron']:
		return'Se tiene que especificar un patron.'
	# Patron 
	if not os.path.exists(values['patron']):
		return 'No se encuentra el fichero del patron.'
	else:
		patron = values['patron']
	# Patron 
	if not values['salida']:
		return 'Se tiene que especificar un PATH del fichero de salida.'
	else:
		salida = values['salida']
	
	if values['blancas'] == False and  values['negras'] == False:
		ganador = 0
	elif values['blancas'] == True and  values['negras'] == False:
		ganador = 1
	elif values['blancas'] == False and  values['negras'] == True:
		ganador = 2
	elif values['blancas'] == True and  values['negras'] == True:
		ganador = 3
		
	if values['img'] == True:
		formasal = 0
	else:
		formasal = 1
	
	fileid = dateini.split('/')
	fileidmin = int(fileid[0])*12 + int(fileid[1])
	fileid = datefin.split('/')
	fileidmax = int(fileid[0])*12 + int(fileid[1])
	# grabar fichero conf/job.conf
	resultado = subprocess.run('echo "ELOMIN='+ str(elomin) +'" > ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
	if resultado.returncode == 0:
		resultado = subprocess.run('echo "ELOMAX='+ str(elomax) +'" >> ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
		if resultado.returncode == 0:
			resultado = subprocess.run('echo "GANADOR='+ str(ganador) +'" >> ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
			if resultado.returncode == 0:
				resultado = subprocess.run('echo "FORMASAL='+ str(formasal) +'" >> ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
				if resultado.returncode == 0:
					resultado = subprocess.run('echo "PATRON='+ str(patron) +'" >> ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
					if resultado.returncode == 0:
						resultado = subprocess.run('echo "FIFO='+ str(FIFO) +'" >> ' + PATHAJEDREZ + '/conf/job.conf', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
						if resultado.returncode < 0:
							print("Error al al grabar FIFO en job.conf.\n" + resultado.stderr)
					else:
						print("Error al al grabar PATRON en job.conf.\n" + resultado.stderr)
				else:
					print("Error al al grabar FORMASAL en job.conf.\n" + resultado.stderr)
			else:
				print("Error al al grabar GANADOR en job.conf.\n" + resultado.stderr)
		else:
			print("Error al al grabar ELOMAX en job.conf.\n" + resultado.stderr)
	else:
		print("Error al al grabar ELOMIN en job.conf.\n" + resultado.stderr)
	
	return ''

# funcion para abortar el job en curso en HADOOP.	
def funkill() :
	global ejecucion
	window["fase"].update("Abortando..")
	event, values = window.read(timeout = 40)
	jobname=''
	# buscar codigo de trabajo hadoop  job_xxxx en el resultado
	resultado = subprocess.run('hadoop job -list | grep job_', shell=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE, text=True)
	if resultado.returncode == 0:
		if len(str(resultado.stdout)) > 0:
			aux = resultado.stdout.split()
			print(aux)
			jobname = aux[0]
			
			# matar el job en curso codigo obtenido anteriormente.
			resultado = subprocess.run('hadoop job -kill ' + jobname, shell=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE, text=True)
			if resultado.returncode == 0:
				print("Job eliminado.\n" + resultado.stdout)
			else:
				print("Error al eliminar el job.\n" + resultado.stderr)		
	else:
		print("Error al eliminar el job.\n" + resultado.stderr)	# No hay Job en curso
	#inicia valores ventana
	window["encontrados"].update(str(0))
	window["pbar"].update(str(0))
	window["pbarval"].update(str(0)+'%')
	
	# Cambia boton a START en verde.
	l18.Update(text="Start",button_color="green")
	
	# Borra info fase.
	window["fase"].update("")
	ejecucion = False
	return


# funcion para preparar y lanzar el sistema de busqueda.
def funstart() :
	global ejecucion
	global particiones
	global partidas
	global hallados
	global vueltas
	global nlineas
	

	# Indicacion fase posible detencion Job en curso
	window["fase"].update("abortando posible instancia en ejecucion..")
	event, values = window.read(timeout = 40)
	funkill()
	
	# query a tabla particiones con fileidmin fileidmax volcando a dat/entrada.txt
	window["fase"].update("Determinando Particiones..")
	event, values = window.read(timeout = 40)

	resultado = subprocess.run('sellistapart ' + PATHAJEDREZ + '/base/base_0/'+ BASENAME + ' ' + str(fileidmin) + ' ' + str(fileidmax) + ' > ' \
										+ PATHAJEDREZ + '/data/entrada.txt', shell=True,stderr=subprocess.PIPE, text=True)
	if resultado.returncode == 0:		# Query OK.
		# contamos n lineas de entrada.txt para luego calcular progreso.
		resultado = subprocess.run('wc -l ' + PATHAJEDREZ + '/data/entrada.txt', shell=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE, text=True)
		if resultado.returncode == 0:		# cuenta lineas OK.
			aux = resultado.stdout.split()
			nlineas = int(aux[0])
			
			# borrar el entrada.txt que pudiera haber en HDFS de una ejecucion anterior
			window["fase"].update("Borrando posible huella anterior..")
			event, values = window.read(timeout = 40)
			resultado = subprocess.run('hadoop fs -rm entrada.txt', shell=True, text=True)
			
			# copiamos data/entrada.txt a HDFS
			resultado = subprocess.run('hadoop fs -copyFromLocal ' + PATHAJEDREZ + '/data/entrada.txt entrada.txt', shell=True,stderr=subprocess.PIPE, text=True)
			if resultado.returncode == 0:	# copia entrada.txt OK.
				
				# borrar la carpeta resajedrez en HDFS donde depositara resultado ejecucion job
				resultado = subprocess.run('hadoop fs -rm -r resajedrez', shell=True, text=True)
				
				# borrar todas las salidas de una posible ejecucion anterior.
				resultado = subprocess.run('rm -r ' + PATHAJEDREZ + '/data/salida/*', shell=True, text=True)

				# Inicia indicadores progreso.
				window["pbar"].update("0")
				window["pbarval"].update("0%")
				window["encontrados"].update("0 0 0")
				
				#inicia totalizadores.
				particiones = 0
				partidas = 0
				hallados = 0
				vueltas = 0
				
				# flush cola de mensajes.
				while q1.qsize() > 0:
					msg=q1.get()
				empieza = 1 	# activa tratamiento cola de mensajes.
				
				# ejecutar job en hadoop, en background.
				os.system('hadoop jar ' + HADOOP_HOME + '/share/hadoop/tools/lib/hadoop-streaming-3.3.4.jar -files '\
							+ PATHAJEDREZ + '/bin/mapbpatronsql -mapper ' + PATHAJEDREZ + '/bin/mapbpatronsql  -input ./entrada.txt -output ./resajedrez &')
				window["fase"].update("Lanzando Job..")
				event, values = window.read(timeout = 40)
				sleep(15);	# espera a lanzamiento Job en HADOOP.
				
				# Cambia boton a parada y en rojo y marca en ejecucion.
				l18.Update(text="Stop",button_color="red")
				window["fase"].update("En Ejecución..")
				ejecucion = True		
			else:
				print("Error al copiar el entrada.txt a HDFS.\n" + resultado.stderr)								
		else:
			print("Error al contar el número de lineas.\n" + resultado.stderr)	
	else:
		print("Error query tabla de particiones.\n" + resultado.stderr)				
	return


# funcion para generar salida final.
# copia todos los ficheros de particion al fichero de resultado final.
def fungensal() :
	global salida
	
	# info de progreso.
	window["fase"].update("Generando salida..")
	event, values = window.read(timeout = 40)
	
	# copia ficheros a resultado.
	os.system('cat '+ PATHAJEDREZ + '/data/salida/*/* > ' + salida)
	
	# trabajo finalizado, cambiamos boton a start y verde.
	window["fase"].update("Finalizado..")
	l18.Update(text="Start",button_color="green")
	return

start_hadoop()	# comprobamos que hadoop esta lanzado y lanzamos en caso contrario.

# Bucle de eventos y proceso de mensajes de la cola.
while True:
	event, values = window.read(timeout = 40)	# timeout de 40 ms.
	
	if event == sg.WIN_CLOSED : # si el usuario cierra la ventana
		break
	elif event == "Start" :		# pulsado el boton START.
		if ejecucion == False :	# No esta en ejecucion
			mensaje = validar(values)
			if mensaje == '':		# validacion correcta.
				funstart()
				empieza = 0;
			else:						# falla la validacion de algun campo.
				# Crear la ventana emergente personalizada
				popup_window = sg.Window('Advertencia',layout = [[sg.Text(mensaje)], [sg.Button('Cerrar')]]\
									, keep_on_top=True, finalize=True,location=(screen_width/2+200, screen_height/2+200))
				while True:			# bucle de eventos ventana emergente.
					event_2, values_2 = popup_window.read()
					if event_2 == sg.WIN_CLOSED or event_2 == 'Cerrar':
						popup_window.close()  	# Cerrar la ventana 2
						break
		else:		# en ejecucion, aborta job en curso.
			funkill()
					
	if ejecucion == True :	# en ejecucion, atendemos cola de mensajes.
		while q1.qsize() > 0:	# hay elementos en la cola.
			empieza = 1
			msg=q1.get()
			msgd=msg.decode("utf-8")
			x = msgd.split(",")
			particiones = particiones + int(x[0])
			partidas = partidas + int(x[1])
			hallados = hallados + int(x[2])
			
		if empieza > 0 :		# control de vida, una vez que empieza, 4 s. sin mensajes => terminado
			empieza = empieza + 1
			if empieza > 100 : # 4 s. sin mensajes => final, generamos salida.
				fungensal()			
				ejecucion = False
				
		vueltas = vueltas + 1	# contador para refresco ventana
		if vueltas > 10 :			# refresca valores cada 400 ms.
			vueltas = 0
			resultado = str(particiones)+'  '+str(partidas)+'  '+str(hallados)
			progreso = particiones * 100// nlineas
			window["encontrados"].update(resultado)
			window["pbar"].update(str(progreso))
			window["pbarval"].update(str(progreso)+'%')

window.close()
