import React from "react";

interface KnobProps {
  size?: number;
  min?: number;
  max?: number;
  step?: number;
  value: number;
  onChange: (value: number) => void;
  label?: string;
  displayValue?: boolean;
  sensitivity?: number; // pixels per full range
}

interface KnobState {
  dragging: boolean;
}

export default class Knob extends React.Component<KnobProps, KnobState> {
  static defaultProps = {
    size: 80,
    min: 0,
    max: 100,
    step: 1,
    label: "",
    displayValue: false,
    sensitivity: 200, // 200px for full sweep
  };

  private svgRef = React.createRef<SVGSVGElement>();
  private startY = 0;
  private startValue = 0;

  constructor(props: KnobProps) {
    super(props);
    this.state = { dragging: false };
  }

  componentDidMount() {
    window.addEventListener("mousemove", this.onMouseMove);
    window.addEventListener("mouseup", this.onMouseUp);
  }

  componentWillUnmount() {
    window.removeEventListener("mousemove", this.onMouseMove);
    window.removeEventListener("mouseup", this.onMouseUp);
  }

  onMouseDown = (e: React.MouseEvent) => {
    e.preventDefault();
    this.startY = e.clientY;
    this.startValue = this.props.value;
    this.setState({ dragging: true });
  };

  onMouseUp = () => {
    if (this.state.dragging) {
      this.setState({ dragging: false });
    }
  };

  onMouseMove = (e: MouseEvent) => {
    if (!this.state.dragging) return;

    const {
      min = 0,
      max = 100,
      step = 1,
      sensitivity = 200,
      onChange,
    } = this.props;

    // Calculate change based on vertical drag distance
    const dy = this.startY - e.clientY;
    const range = max - min;
    const deltaValue = (dy / sensitivity) * range;
    let newValue = this.startValue + deltaValue;

    // Snap to step
    newValue = Math.round(newValue / step) * step;
    // Clamp
    newValue = Math.max(min, Math.min(max, newValue));

    if (newValue !== this.props.value) {
      onChange(newValue);
    }
  };

  render() {
    const {
      size = 80,
      min = 0,
      max = 100,
      value,
      label,
      displayValue,
    } = this.props;

    const pct = (value - min) / (max - min);
    const angle = pct * 352;
    const drawRaw = (270 + angle) % 360;

    const indicatorLength = (size - 20) / 3;
    const theta = (drawRaw * Math.PI) / 180;
    const x2 = size / 2 + Math.cos(theta) * indicatorLength;
    const y2 = size / 2 + Math.sin(theta) * indicatorLength;

    return (
      <div className="flex flex-col items-center select-none">
        {label && (
          <center>
            <div className="mt-2 text-sm font-medium text-black">{label}</div>
            <br />
          </center>
        )}
        <svg
          ref={this.svgRef}
          width={size}
          height={size}
          onMouseDown={this.onMouseDown}
          className="cursor-row-resize"
        >
          <circle
            cx={size / 2}
            cy={size / 2}
            r={(size - 10) / 6}
            stroke="#444"
            strokeWidth="10"
            fill="none"
          />
          <line
            x1={size / 2}
            y1={size / 2}
            x2={x2}
            y2={y2}
            stroke="#fff"
            strokeWidth="8"
            strokeLinecap="round"
          />
          <circle cx={size / 2} cy="10%" r={4} fill="#fff" />

          {displayValue && (
            <text
              x="50%"
              y="90%"
              dominantBaseline="middle"
              textAnchor="middle"
              fontSize={size / 5}
              fill="#fff"
            >
              {value}
            </text>
          )}
        </svg>
      </div>
    );
  }
}
