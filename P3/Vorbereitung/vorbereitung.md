### Aufgabe 1

Es reicht nicht die Geschwindigkeit anzupassen, wenn der Sensor
durchfahren wird, denn dann ist es möglicherweise schon zu spät. Hier
ist es möglich schon vor einer Kurve, also dem Eintritt des Ereignisses,
abzubremsen. Außerdem ist es so möglich vorher stärker zu beschleunigen.

### Aufgabe 2

```C
fahre_segment(unsigned int type, unsigned int laenge) {
    schlafzeit = laenge * 0.8;

    if(innen) {
        switch(type) {
            case GERADE:
                speed += 10;
                set_speed();
                schlafen();
                speed -= 10;
                set_speed();
                break;
            case GEFAHR:
                speed -= 10;
                set_speed();
                schlafen();
                speed += 10;
                set_speed();
                break;
            case KURVE:
                speed -= 10;
                set_speed();
                schlafen();
                speed += 10;
                set_speed();
                break;
        }
    } else {
        switch(type) {
            case GERADE:
                speed += 10;
                set_speed();
                schlafen();
                speed -= 10;
                set_speed();
                break;
            case GEFAHR:
                speed -= 10;
                set_speed();
                schlafen();
                speed += 10;
                set_speed();
                break;
            case KURVE:
                speed -= 10;
                set_speed();
                schlafen();
                speed += 10;
                set_speed();
                break;
        }
    }
}
```
