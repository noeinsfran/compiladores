#Tarea 1
#Integrantes: 
	#Noelia Insfrán
	#Bruno Brizuela

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int es_delimitador(char c) {
    return isspace(c) || c == ',' || c == ':' || c == '}' || c == ']' || c == '{' || c == '[';
}

void analizar_linea(char *linea, FILE *salida, int nro_linea) {
    char *p = linea;

    while (*p) {

        if (isspace(*p)) {
            p++;
            continue;
        }

        // Símbolos
        if (*p == '{') { fprintf(salida, "L_LLAVE "); p++; }
        else if (*p == '}') { fprintf(salida, "R_LLAVE "); p++; }
        else if (*p == '[') { fprintf(salida, "L_CORCHETE "); p++; }
        else if (*p == ']') { fprintf(salida, "R_CORCHETE "); p++; }
        else if (*p == ',') { fprintf(salida, "COMA "); p++; }
        else if (*p == ':') { fprintf(salida, "DOS_PUNTOS "); p++; }

        // Strings
        else if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) p++; // manejar escape simple
                p++;
            }
            if (*p == '"') {
                fprintf(salida, "LITERAL_CADENA ");
                p++;
            } else {
                fprintf(salida, "ERROR_LEXICO(linea %d)", nro_linea);
                return;
            }
        }

        // Números (enteros, decimales, exponenciales)
        else if (isdigit(*p)) {
            char *inicio = p;

            while (isdigit(*p)) p++;

            if (*p == '.') {
                p++;
                if (!isdigit(*p)) {
                    fprintf(salida, "ERROR_LEXICO(linea %d)", nro_linea);
                    return;
                }
                while (isdigit(*p)) p++;
            }

            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') p++;
                if (!isdigit(*p)) {
                    fprintf(salida, "ERROR_LEXICO(linea %d)", nro_linea);
                    return;
                }
                while (isdigit(*p)) p++;
            }

            fprintf(salida, "LITERAL_NUM ");
        }

        // Palabras clave
        else if (strncmp(p, "true", 4) == 0 && es_delimitador(p[4])) {
            fprintf(salida, "PR_TRUE ");
            p += 4;
        }
        else if (strncmp(p, "TRUE", 4) == 0 && es_delimitador(p[4])) {
            fprintf(salida, "PR_TRUE ");
            p += 4;
        }
        else if (strncmp(p, "false", 5) == 0 && es_delimitador(p[5])) {
            fprintf(salida, "PR_FALSE ");
            p += 5;
        }
        else if (strncmp(p, "FALSE", 5) == 0 && es_delimitador(p[5])) {
            fprintf(salida, "PR_FALSE ");
            p += 5;
        }
        else if (strncmp(p, "null", 4) == 0 && es_delimitador(p[4])) {
            fprintf(salida, "PR_NULL ");
            p += 4;
        }
        else if (strncmp(p, "NULL", 4) == 0 && es_delimitador(p[4])) {
            fprintf(salida, "PR_NULL ");
            p += 4;
        }

        // Error léxico
        else {
            fprintf(salida, "ERROR_LEXICO(linea %d)", nro_linea);
            return;
        }
    }

    fprintf(salida, "\n");
}

int main() {
    FILE *fuente = fopen("fuente.txt", "r");
    FILE *salida = fopen("output.txt", "w");

    char linea[1024];
    int nro_linea = 1;

    if (!fuente || !salida) {
        printf("Error al abrir archivos.\n");
        return 1;
    }

    while (fgets(linea, sizeof(linea), fuente)) {
        analizar_linea(linea, salida, nro_linea);
        nro_linea++;
    }

    fclose(fuente);
    fclose(salida);

    printf("Analisis completado correctamente.\n");
    return 0;
}
