### Aufgabe 1

Ja, warum eigentlich?

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
