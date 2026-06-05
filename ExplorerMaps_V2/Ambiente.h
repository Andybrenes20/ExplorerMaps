#pragma once

enum class ModoAmbiente {
    MANUAL_DIA,
    MANUAL_NOCHE,
    AUTOMATICO
};

struct ConfigAmbiente {
    ModoAmbiente modoActual = ModoAmbiente::AUTOMATICO;
    float horaDelDia = 12.0f;       // De 0.0 a 24.0 (12 = Mediodía)
    float velocidadTiempo = 1.0f;    // Qué tan rápido pasa el día en modo automático

    // Valores de iluminación resultantes que enviaremos al Shader
    float intensidadAmbiental = 0.4f;
    float intensidadDifusa = 1.0f;
};