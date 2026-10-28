// Verity - cosas que cambian según el sistema (dónde se guarda la partida, la hora, márgenes).
#pragma once

#include <cstdint>
#include <string>

namespace verity::platform {

std::string savePath();                                                  // archivo de guardado
std::string readFile(const std::string& path);                           // "" si no existe
bool writeFileAtomic(const std::string& path, const std::string& data);  // escribe sin dejar archivos a medias
int64_t unixNow();                                                       // segundos desde 1970
int64_t dayNumber();                                                     // número del día local (cambia a medianoche)
float safeTop();                                                         // margen para muescas, en unidades lógicas

}  // namespace verity::platform
