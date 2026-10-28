# Cómo hacer el APK de Android (sin instalar nada en tu PC)

Tu laptop no necesita programas pesados: **GitHub compila el APK por ti, gratis, en sus computadoras.**
Solo necesitas una cuenta de GitHub y un navegador.

## Paso a paso

1. **Crea una cuenta** en <https://github.com> (gratis).
2. **Crea un repositorio nuevo**: botón verde "New" (o el "+" de arriba > *New repository*).
   Ponle de nombre `verity`. Puedes dejarlo **Private** (privado). Pulsa *Create repository*.
3. **Sube el proyecto**:
   - Descomprime `Verity-proyecto-codigo.zip` en una carpeta.
   - En la página del repositorio pulsa **uploading an existing file**
     (o *Add file > Upload files*).
   - Abre la carpeta que descomprimiste, selecciona **todo lo de adentro** (Ctrl + A) y arrástralo a la página.
     Asegúrate de que también suba la carpeta `.github` y la carpeta `android`.
   - Espera a que termine de subir y pulsa **Commit changes**.
4. **Mira cómo compila**: abre la pestaña **Actions**. Verás "Compilar Verity" trabajando (amarillo).
   Tarda unos 10 a 15 minutos la primera vez. Cuando termine con una palomita verde, entra a esa ejecución.
5. **Descarga el APK**: abajo, en la sección **Artifacts**, pulsa **Verity-Android-APK**. Se baja un `.zip`;
   descomprímelo y adentro está `app-debug.apk`.
6. **Instálalo en tu teléfono**: pásalo por cable USB, WhatsApp o Drive. Al abrirlo, Android te pedirá
   permitir "instalar apps de fuentes desconocidas": acepta solo para ese archivo.

Los anuncios que verás son de **prueba** (dicen "Test Ad"). Así debe ser por ahora.

## Si algo sale mal

- **Si no aparece nada en Actions**: seguramente no subió la carpeta `.github`. Crea el archivo a mano:
  *Add file > Create new file*, escribe el nombre `.github/workflows/build.yml` (con las barras, GitHub crea las carpetas)
  y pega adentro el contenido del archivo `.github/workflows/build.yml` del zip. Luego *Commit changes*.
- **Si la ejecución sale con una X roja**: ábrela, pulsa el paso que falló (el que tiene la X), copia las
  últimas líneas del mensaje y **pégamelas aquí**. Lo arreglo y te mando el proyecto corregido.
  Es normal que la primera compilación de Android necesite algún ajuste.
- Con el mismo flujo también salen los `.exe` de Windows (pestaña Actions > Artifacts).

## Para ganar dinero con los anuncios (después)

El APK de arriba no gana dinero porque usa anuncios de prueba. Cuando tengas tu cuenta de AdMob, cambia los 3
códigos en `android/app/src/main/res/values/ads.xml` (puedes editarlo en GitHub: abre el archivo y pulsa el lápiz).
Los pasos completos están en el `README.md`, sección "Ganar dinero con AdMob".
