import React from 'react';
import { Container } from '@material-ui/core';
import { Switcher } from './components/Switcher';

const switches = [
    { label: "Jets 1", param: "JETS1" },
    { label: "Jets 2", param: "JETS2" },
    { label: "Air", param: "AIR" },
    { label: "Clean", param: "CLEAN" },
];

function App() {
    return (
        <div style={{ backgroundColor: "#cccccc" }}>
            <Container style={{ backgroundColor: "white" }} maxWidth='xs'>
                {switches.map(s => <Switcher key={s.label} {...s} />)}
            </Container>
        </div>
    );
}

export default App;
