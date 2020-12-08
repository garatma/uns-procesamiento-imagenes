#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <math.h>
#include <sys/resource.h>

#define BLANCO 			   255
#define CANT_GALLETITAS    6
#define ARRIBA             0
#define ABAJO              1
#define CENTRO             2
#define IZQUIERDA          3
#define DERECHA            4
#define MAX_DIVISOR		   300
#define MAX_UMBRAL_COLOR   50
#define MAX_UMBRAL_FONDO   50

#define MAX_DP 51
#define MAX_MINDIST 11
#define MAX_PARAM1 201
#define MAX_PARAM2 201
#define MAX_MINRADIUS 11
#define MAX_MAXRADIUS 11
#define MAX_RHO 101
#define MAX_THETA 361
#define MAX_THRESHOLD 101
#define MAX_MINLINELENGHT 151
#define MAX_MAXLINEGAP 201

int umbral_galletita, divisor_umbral = 200, umbral_color = 22, umbral_fondo = 22;
int dp = 20, minDist = 2, param1 = 100, param2 = 100, minRadius = 2, maxRadius = 7;
int rho = 1, theta = 180, threshold = 50, minLineLength = 50, maxLineGap = 10;
uchar fondo[3];

bool opcion_o = false;
struct galletita {
	std::string nombre;
	uchar color[3];
	int cantidad;
	bool rectangular;
};

struct argumento_callback {
	cv::Mat imagen;
	cv::Mat grises;
	// std::string ventana_resultados = "Resultados";
};


void inicializar_galletitas(struct galletita galletitas[CANT_GALLETITAS])
{
	galletitas[0] = { "Anillos chocolate" , {30  , 36  , 51}  , 0 , false};
	galletitas[1] = { "Amarillas"         , {68  , 120 , 143} , 0 , false};
	galletitas[2] = { "Rosas"             , {104 , 118 , 147} , 0 , false};
	galletitas[3] = { "Rectangulares"     , {30  , 36  , 51}  , 0 , true};
	galletitas[4] = { "Pepitos"   		  , {37  , 71  , 104} , 0 , true};
	galletitas[5] = { "No identificada"   , {0   , 0   , 0}   , 0 , true};
}

bool es(uchar color1[3], uchar color2[3], int umbral) {
	bool es = true;
	int i = 0;
	while (es && i < 3) {
		es = abs(color1[i]-color2[i]) <= umbral;
		i++;
	}
	return es;
}

// TODO: retocar esto. ver qué retornar
int es_galletita(cv::Mat * imagen, cv::Mat * mapa, int fila, int columna, int pos_pixel_anterior, double suma[3])
{
	uchar * pmapa, * p, color[3];
	int contador = 0;

	p = (*imagen).ptr<uchar>(fila);
	pmapa = (*mapa).ptr<uchar>(fila);
	for (int k = 0; k < (*imagen).channels(); ++k) color[k] = p[columna+k];

	if (!es(color, fondo, umbral_fondo) && pmapa[columna/3] == 0) {
		contador++;
		pmapa[columna/3] = BLANCO;
		for (int k = 0; k < (*imagen).channels(); ++k) {
			suma[k] += int(p[columna+k]);
		}

		// if (contador == umbral_galletita) return contador;

		if (pos_pixel_anterior != ARRIBA && 0 < fila-1) {
			pmapa = (*mapa).ptr<uchar>(fila-1);
			if (pmapa[columna/3] == 0)
				contador += es_galletita(imagen, mapa, fila-1, columna, ABAJO, suma);
		}

		if (pos_pixel_anterior != ABAJO && fila+1 < (*imagen).rows) {
			pmapa = (*mapa).ptr<uchar>(fila+1);
			if (pmapa[columna/3] == 0)
				contador += es_galletita(imagen, mapa, fila+1, columna, ARRIBA, suma);
		}

		// if (contador == umbral_galletita) return contador;

		// TODO: fijarse si anda bien esto del ±3
		if (pos_pixel_anterior != IZQUIERDA && -1 < columna-3 && pmapa[columna/3-1] == 0)
			contador += es_galletita(imagen, mapa, fila, columna-3, DERECHA, suma);

		// if (contador == umbral_galletita) return contador;

		if (pos_pixel_anterior != DERECHA && columna+3 < (*imagen).cols && pmapa[columna+1] == 0)
			contador += es_galletita(imagen, mapa, fila, columna+3, IZQUIERDA, suma);

		// if (contador == umbral_galletita) return contador;
	}

	return contador;
}

int obtener_indice_galletita(struct galletita galletitas[CANT_GALLETITAS], uchar color[3])
{
	bool encontre = false;
	int i = 0;
	while (!encontre && i < CANT_GALLETITAS-1) {
		encontre = es(galletitas[i].color, color, umbral_color);
		i++;
	}
	if (encontre) i--;
	return i;
}

void encontrar_galletitas(int, void * argumento)
{
	struct argumento_callback * p_argumento = (struct argumento_callback *) argumento;

	cv::Mat imagen, grises;
	imagen = p_argumento->imagen.clone();
	grises = p_argumento->grises.clone();

	// datos sobre las galletitas
	struct galletita galletitas[CANT_GALLETITAS];
	inicializar_galletitas(galletitas);

	// un mapa que indique qué píxeles se leyeron (en BLANCO)
	cv::Mat mapa = cv::Mat::zeros(imagen.rows, imagen.cols, CV_8U);

	if (!divisor_umbral) divisor_umbral++;
	umbral_galletita = imagen.cols*imagen.rows*imagen.channels()/divisor_umbral;

	uchar * pmapa, * p, color[3], promedio[3];
	double suma[3], pixeles_galletita;
	int galletitas_encontradas = 0, indice;

	// recorrer imagen buscando píxeles de color distinto al fondo y ver cuántos
	// hay de estos de manera contigua para determinar si es una galletita o no
	for (int i = 1; i < imagen.rows; ++i) {
		p = imagen.ptr<uchar>(i);
		pmapa = mapa.ptr<uchar>(i);
		for (int j = 0; j < imagen.cols*imagen.channels(); j += 3) {
			for (int k = 0; k < imagen.channels(); ++k) color[k] = p[j+k];
			if (!pmapa[j] && !es(color, fondo, umbral_fondo)) {
				// si no se procesó el pixel y no es fondo
				for (int k = 0; k < imagen.channels(); ++k) suma[k] = 0;
				pixeles_galletita = es_galletita(&imagen, &mapa, i, j, CENTRO, suma);
				if (pixeles_galletita > umbral_galletita) {
					// la cantidad de píxeles contiguos del mismo color es mayor
					// que el mínimo necesario para concluir que es una galletita
					galletitas_encontradas++;
					for (int k = 0; k < imagen.channels(); ++k) {
						promedio[k] = suma[k]/pixeles_galletita;
					}
					indice = obtener_indice_galletita(galletitas, promedio);
					galletitas[indice].cantidad++;
				}
			}
		}
	}

	// detectar círculos
	cv::GaussianBlur(grises, grises, cv::Size(9, 9), 2, 2);
	std::vector <cv::Vec3f> circulos;
	cv::HoughCircles(grises, circulos, cv::HOUGH_GRADIENT, (dp+1)/10.0, grises.cols/(10-minDist+1), param1, param2, grises.cols/(10-minRadius+1), grises.cols/(10-maxRadius+1));
	// detectar líneas (rectángulos)
	std::vector <cv::Vec4i> lineas;
	cv::Mat canny;
	Canny(imagen, canny, 50, 200, 3);
	cv::HoughLinesP(canny, lineas, (rho+1)/10.0, CV_PI/(theta+1), threshold, minLineLength, maxLineGap);

	// dibujar círculos
	for(size_t i = 0; i < circulos.size(); i++)
	{
		cv::Point center(cvRound(circulos[i][0]), cvRound(circulos[i][1]));
		int radius = cvRound(circulos[i][2]);
		circle(imagen, center, 3, cv::Scalar(0,255,0), -1, 8, 0);
		circle(imagen, center, radius, cv::Scalar(0,0,255), 3, 8, 0);
	}

	// dibujar líneas
	for(size_t i = 0; i < lineas.size(); i++)
	{
		cv::Vec4i l = lineas[i];
		line(imagen, cv::Point(l[0], l[1]), cv::Point(l[2], l[3]), cv::Scalar(0,255,0), 3, cv::LINE_AA);
	}

	std::string resultados = "Resultados: ";
	cv::Mat imagen_resultados = cv::Mat::zeros(400, 400, CV_8U);
	cv::putText(imagen_resultados, resultados, cv::Point(0, 25),
		cv::FONT_HERSHEY_DUPLEX, 1, BLANCO, 1, cv::LINE_8, false
	);

	int i;
	for (i = 0; i < CANT_GALLETITAS; ++i) {
		resultados = "    ";
		resultados = resultados + galletitas[i].nombre;
		resultados = resultados + ": ";
		resultados = resultados + std::to_string(galletitas[i].cantidad);
		cv::putText(imagen_resultados, resultados, cv::Point(0, 75+50*i),
			cv::FONT_HERSHEY_DUPLEX, 1, BLANCO, 1, cv::LINE_8, false
		);
	}

	resultados = "    Total: ";
	resultados = resultados + std::to_string(galletitas_encontradas);
	cv::putText(imagen_resultados, resultados, cv::Point(0, 75+50*i),
			cv::FONT_HERSHEY_DUPLEX, 1, BLANCO, 1, cv::LINE_8, false
	);

	namedWindow("Resultados", cv::WINDOW_FREERATIO);
	cv::resizeWindow("Resultados", 450, 700);
	cv::moveWindow("Resultados", 1, 1);
	imshow("Resultados", imagen_resultados);

	if (opcion_o) {
		namedWindow("Procesamiento pixeles", cv::WINDOW_FREERATIO);
		cv::resizeWindow("Procesamiento pixeles", 450, 450);
		cv::moveWindow("Procesamiento pixeles", 451, 0);
		imshow("Procesamiento pixeles", mapa);

		namedWindow("Canal H", cv::WINDOW_FREERATIO);
		cv::resizeWindow("Canal H", 450, 450);
		cv::moveWindow("Canal H", 931, 0);
		imshow("Canal H", imagen);
	}
}

int main(int argc, char ** argv)
{
	// incrementar el tamaño de la pila para soportar fotos de mayor resolución
	struct rlimit rlp;
	rlp.rlim_cur = RLIM_INFINITY;
	rlp.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_STACK, &rlp);

	struct argumento_callback argumento_callback;

	cv::Mat imagen;

	// obtener imagen
	if (argc == 3) {
		opcion_o = !strcmp("-o", argv[1]);
		if (!opcion_o)
			std::cout << std::endl << "Ejecute con la opción -o para mostrar el procesamiento de píxeles" << std::endl;
		imagen = imread(argv[2], cv::IMREAD_COLOR);
	}
	else if (argc == 2) {
		std::cout << std::endl << "Ejecute con la opción -o para mostrar el procesamiento de píxeles" << std::endl;
		imagen = imread(argv[1], cv::IMREAD_COLOR);
	}
	else {
		std::cout << "Debe especificar la ruta a una imagen." << std::endl;
		return EXIT_FAILURE;
	}

	if(imagen.empty()) {
		std::cout << "No se pudo encontrar la imagen." << std::endl;
		return EXIT_FAILURE;
	}

	argumento_callback.imagen = imagen.clone();
	cv::cvtColor(imagen, argumento_callback.grises, cv::COLOR_BGR2GRAY);

	// variables necesarias
	uchar * p;

	// establecer color del fondo a partir del promedio de colores de los
	// píxeles de la primera fila
	double suma[3] = {0, 0, 0};
	p = imagen.ptr<uchar>(0);
	for (int j = 0; j < imagen.cols * imagen.channels(); j+=3) {
		for (int i = 0; i < 3; ++i) suma[i] += p[j+i];
	}

	for (int i = 0; i < 3; ++i) {
		fondo[i] = suma[i]/(imagen.cols);
	}

	encontrar_galletitas(0,&argumento_callback);

	const char * barra_galletita = "Umbral píxeles galletita",
		* barra_color = "Umbral color galletita",
		* barra_fondo = "Umbral color fondo",
		* barra_dp = "dP",
		* barra_minDist = "Distancia entre círculos",
		* barra_minRadius = "Radio mínimo",
		* barra_maxRadius= "Radio máximo",
		* barra_rho = "Rho",
		* barra_theta = "Tita",
		* barra_threshold = "Umbral",
		* barra_minLineLength = "Mínima longitud línea",
		* barra_maxLineGap = "Máximo espacio entre líneas";

	cv::createTrackbar(barra_galletita, "Resultados", &divisor_umbral, MAX_DIVISOR, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_color, "Resultados", &umbral_color, MAX_UMBRAL_COLOR, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_fondo, "Resultados", &umbral_fondo, MAX_UMBRAL_FONDO, encontrar_galletitas, &argumento_callback);

	cv::createTrackbar(barra_dp, "Resultados", &dp, MAX_DP, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_minDist, "Resultados", &minDist, MAX_MINDIST, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_minRadius, "Resultados", &minRadius, MAX_MINRADIUS, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_maxRadius, "Resultados", &maxRadius, MAX_MAXRADIUS, encontrar_galletitas, &argumento_callback);

	cv::createTrackbar(barra_rho, "Resultados", &rho, MAX_RHO, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_theta, "Resultados", &theta, MAX_THETA, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_threshold, "Resultados", &threshold, MAX_THRESHOLD, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_minLineLength, "Resultados", &minLineLength, MAX_MINLINELENGHT, encontrar_galletitas, &argumento_callback);
	cv::createTrackbar(barra_maxLineGap, "Resultados", &maxLineGap, MAX_MAXLINEGAP, encontrar_galletitas, &argumento_callback);

	cv::waitKey();

	return EXIT_SUCCESS;
}
