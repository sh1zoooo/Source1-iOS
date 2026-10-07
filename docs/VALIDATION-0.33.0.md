# 0.33.0 / build 36 — BSP entity model preview

Оценка ~92% минимального демонстрационного этапа, не полного Source/CS:S.

- BSP `prop_dynamic` / `prop_dynamic_override` читаются как данные, без запуска
  entity factories, I/O, scripts или game DLL. До 512 visual model candidates.
- Проверяются model path, origin, angles/yaw, integer skin 0–255, scale >0 и
  <=16. Используется existing bounded MDL44/48/49 + VVD/VTX + VMT/VTF путь.
- Модель отображается в bind pose. Bodygroup !=0 пропускается; collision,
  entity animation, parent hierarchy, render effects и outputs не реализованы.
- Fixture содержит 2 instances одного two-material model с разными skins,
  поворотом и масштабом. Проверяется staging/material slots и отсутствие
  выдуманных physical objects. Malformed entity не заменяет текущую сцену.
- Local ASan/UBSan: 30 000 spawn/model mutations, 11 751 accepted,
  18 249 rejected; обе output collections сохраняются при отказе.

CI ожидается: Linux 5/5; ARM64 IPA; simulator 2×114 startup +18 старых live
checks +2 entity checks =248 PASS, GPU revision 10. Fixture entity-сцена
проверяется до возврата к skin/HDR/PHY demo; финальный screenshot не показывает
entity fixture. Совместимость реальных dynamic props из кеша пока не проверена.

0.33.0 CI выявил ошибку ожидаемых material slots в новом live entity test:
fixture family 0 задаёт {1,0}, но тест ожидал {0,1}. 0.33.1/build 37 исправляет
ожидания, сохраняя renderer/skin remap. Для simctl launch timeout добавлена
одна bounded retry при отсутствии startup log; успешность по-прежнему требует
все 248 PASS, завершённый GPU кадр и lifecycle evidence. Повторный CI ожидается.
