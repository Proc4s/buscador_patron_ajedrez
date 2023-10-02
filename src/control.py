#!/usr/bin/python
import PySimpleGUI as sg
import datetime
import os
import sys
import tkinter as tk
from ipcqueue import posixmq
from ipcqueue.serializers import RawSerializer
from time import sleep

# cojer variables de entorno $PATHAJEDREZ y $HADOOP_HOME
particiones = 0
partidas = 0
hallados = 0
empieza = 0
ejecucion = False
vueltas = 0

q1 = posixmq.Queue('/ajedrezmsg',serializer=RawSerializer)
os.chmod('/dev/mqueue/ajedrezmsg',0o666)

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
l18 = sg.Button("Start")														# Boton comienzo de tarea
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

# !!!!!comprobar si hadoop esta rodando jps y mirar n lineas.
# !!!!!si no esta rodando start-all.sh y esperar su finalizacion


# Función para validar el formato de fecha
def es_fecha_valida(fecha_texto):
    try:
        datetime.datetime.strptime(fecha_texto, '%Y/%m')
        return True
    except ValueError:
        return False

def validar(values):

	# Fecha inicial
	if not es_fecha_valida(values['dateini']):
		return 'Se tiene que especificar una fecha inicial.'
	# Fecha final
	if not es_fecha_valida(values['datefin']):
		return 'Se tiene que especificar una fecha final.'
	# Fecha inicial > fecha final
	if datetime.datetime.strptime(values['datefin'], '%Y/%m') < datetime.datetime.strptime(values['dateini'], '%Y/%m'):
		return 'La fecha final seleccionada tiene que ser posterior a la fecha inicial.'
	# ELO min
	if not values['elomin'].isdigit():
		return 'Se tiene que especificar un número entero como elo minimo.'
	# ELO min
	if int(values['elomin']) < 0:
		return 'Se tiene que especificar un elo minimo.'
	# ELO max
	if not values['elomax'].isdigit():
		return 'Se tiene que especificar un número entero como elo maximo.'
	# ELO max
	if int(values['elomax']) < 0:
		return'Se tiene que especificar un elo maximo.'
	# ELO min > ELO max
	if int(values['elomax']) < int(values['elomin']):
		return'Se tiene que especificar un elo maximo inferior o igual al elo minimo.'
	# Patron 
	if not values['patron']:
		return'Se tiene que especificar un patron.'
	# Patron 
	if not os.path.exists(values['patron']):
		return 'No se encuentra el fichero del patron.'
	# Patron 
	if not values['salida']:
		return 'Se tiene que especificar un PATH del fichero de salida.'
	return ''

def funstart() :
	# hacer query tabla particiones con fileidmin fileidmax => ejecutando:
	# sellistapart $PATHAJEDREZ/base/base_0/$BASENAME fileidmin fileidmax > data/entrada.txt (puedes generarlo en data)
	# contar el numero de lineas de entrada.txt => wc -l data/entrada.txt
	# borrar el entrada.txt que pudiera haber en HDFS => hadoop fs -rm entrada.txt
	# copiar el entrada.txt a HDFS => hadoop fs -copyFromLocal data/entrada.txt entrada.txt
	# borrar la carpeta HDFS donde vamos a poner los resultados => hadoop fs -rm -r resajedrez
	# borrar todas las salidas de la ejecucion anterior => rm -r salidas/*
	window["fase"].update("En Ejecución..")
	window["pbar"].update("0")
	window["pbarval"].update("0%")
	window["encontrados"].update("0 0 0")
	particiones = 0
	partidas = 0
	hallados = 0
	vueltas = 0
	empieza = 0
	# ejecutar hadoop =>
	# hadoop jar $HADOOP_HOME/share/hadoop/tools/lib/hadoop-streaming-3.3.4.jar -files $PATHAJEDREZ/bin/mapbpatronsql
	# -mapper $PATHAJEDREZ/bin/mapbpatronsql  -input ./entrada.txt -output ./resajedrez &
	ejecucion = True
	return
	
def funkill() :
	window["fase"].update("Abortando..")
	# buscar codigo de trabajo hadoop => hadoop job -list y buscar job_ en el resultado hasta siguiente blanco
	# matar el job => hadoop job -kill codigo obtenido anteriormente.
	ejecucion = False
	return
	
# Event Loop to process "events" and get the "values" of the inputs
while True:
	event, values = window.read(timeout = 40)
	
	if event == sg.WIN_CLOSED : # if user closes window or clicks cancel
		break
	elif event == "Start" :
		if ejecucion == False :
			mensaje = validar(values)
			if mensaje == '':
				funstart()
			else:
				# Crear la ventana emergente personalizada
				popup_window = sg.Window('Advertencia',layout = [[sg.Text(mensaje)], [sg.Button('Cerrar')]], keep_on_top=True, finalize=True,location=(screen_width/2+200, screen_height/2+200))
				while True:
					event_2, values_2 = popup_window.read()
					if event_2 == sg.WIN_CLOSED or event_2 == 'Cerrar':
						popup_window.close()  	# Cerrar la ventana 2
						break
		else:
			funkill()
					
	if ejecucion == True :
		while q1.qsize() > 0:
			empieza = 1
			msg=q1.get()
			msgd=msg.decode("utf-8")
			x = msgd.split(",")
			particiones = particiones + int(x[0])
			partidas = partidas + int(x[1])
			hallados = hallados + int(x[2])
		if empieza > 0 :
			empieza = empieza + 1
			if empieza > 100 :
				window["fase"].update("Finalizado..")
				ejecucion = False
		vueltas = vueltas + 1
		if vueltas > 10 :
			vueltas = 0
			resultado = str(particiones)+'  '+str(partidas)+'  '+str(hallados)
			progreso = particiones * 100//710
			window["encontrados"].update(resultado)
			window["pbar"].update(str(progreso))
			window["pbarval"].update(str(progreso)+'%')

window.close()
